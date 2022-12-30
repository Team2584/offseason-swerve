#include "SwerveConstants.h"
#include "Robot.h"

#include <frc/smartdashboard/SmartDashboard.h>

class SwerveModule
{
private:
  // Instance Variables for each swerve module
  ctre::phoenix::motorcontrol::can::TalonFX *driveMotor, *spinMotor;
  frc::DutyCycleEncoder *magEncoder;
  frc::PIDController *spinPIDController;
  double encoderOffset;
  double driveEncoderInitial;
  double spinEncoderInitialHeading;
  double spinEncoderInitialValue;

public:
  // Constructor for swerve module, setting all instance variables
  SwerveModule(ctre::phoenix::motorcontrol::can::TalonFX *driveMotor_,
               ctre::phoenix::motorcontrol::can::TalonFX *spinMotor_, frc::DutyCycleEncoder *magEncoder_,
               double encoderOffset_)
  {
    driveMotor = driveMotor_;
    spinMotor = spinMotor_;
    magEncoder = magEncoder_;
    encoderOffset = encoderOffset_;
    ResetEncoders();
  }

  // Converts Mag-Encoder Reading to a radian 0 - 2pi
  double GetMagEncoderReading()
  {
    double encoderReading = magEncoder->GetAbsolutePosition();
    // subtract the encoder offset to make 0 degrees forward
    encoderReading -= encoderOffset;
    if (encoderReading < 0)
      encoderReading += 1;
    // Flip the degrees to make clockwise positive
    encoderReading = 1 - encoderReading;
    // Convert from 0-1 to degrees
    encoderReading *= 2 * M_PI;
    return encoderReading;
  }

  void ResetEncoders()
  {
    driveEncoderInitial = driveMotor->GetSelectedSensorPosition();
    spinEncoderInitialHeading = GetMagEncoderReading();
    spinEncoderInitialValue = -1 * spinMotor->GetSelectedSensorPosition();
  }

  // Converts Talon Drive Encoder to Meters
  double GetDriveEncoderMeters()
  {
    return (driveMotor->GetSelectedSensorPosition() - driveEncoderInitial) / 2048 / DRIVE_MOTOR_GEAR_RATIO * DRIVE_MOTOR_CIRCUMFERENCE;
  } 

  // Finds Drive Motor Velocity in Meters per Second
  double GetDriveVelocity()
  {
    return driveMotor->GetSelectedSensorVelocity() / 2048 / 6.54 * 0.10322 * M_PI * 10;
  }

  //Finds Spin Encoder Rotation in Radians
  double GetSpinEncoderRadians()
  {
    //TODO
    double rotation = ((-1 * spinMotor->GetSelectedSensorPosition() - spinEncoderInitialValue) / 2048 / SPIN_MOTOR_GEAR_RATIO * 2 * M_PI) - spinEncoderInitialHeading;
    return fmod(rotation, 2 * M_PI);
  }

  //Stops all motor velocity in swerve module
  void StopSwerveModule()
  {
    spinMotor->Set(ControlMode::PercentOutput, 0);
    driveMotor->Set(ControlMode::PercentOutput, 0);
  }

  //Returns the swerve module's state
  SwerveModuleState GetSwerveModuleState()
  {
    SwerveModuleState state = SwerveModuleState();
    state.speed = units::meters_per_second_t{fabs(GetDriveVelocity())};
    state.angle = Rotation2d(units::radian_t{GetMagEncoderReading() / 180 * M_PI});
    return state;
  }

  //Returns the swerve module's position
  SwerveModulePosition GetSwerveModulePosition()
  {
    SwerveModulePosition state = SwerveModulePosition();
    state.distance = units::length::centimeter_t{GetDriveEncoderMeters() * 100}; // It works, don't judge
    state.angle = Rotation2d(units::radian_t{GetMagEncoderReading()});
    return state;
  }

  // Spin swerve module motors to reach the drive speed and spin angle 
  void DriveSwerveModulePercent(double driveSpeed, double targetAngle)
  {
    // current encoder reading as an angle
    double wheelAngle = GetMagEncoderReading() * 180 / M_PI;
    // amount wheel has left to turn to reach target
    double error = 0;
    // if wheel should spin clockwise(1) or counterclockwise(-1) to reach the target
    int spinDirection = 0;
    // If the drive should spin forward(1) or backward(-1) to move in the correct direction
    int driveDirection = 0;

    // Corrects spin angle to make it positive
    if (targetAngle < 0)
    {
      targetAngle += 360;
    }

    // The below logic determines the most efficient way for the wheel to move to reach the desired angle
    // This could mean moving towards it clockwise, counterclockwise, or moving towards the opposite of the angle
    // and driving in the opposite direction
    if (wheelAngle < targetAngle)
    {
      // if target and wheelangle are less than 90 degrees apart we should spin directly towards the target angle
      if (targetAngle - wheelAngle <= 90)
      {
        error = targetAngle - wheelAngle;
        spinDirection = 1;
        driveDirection = 1;
      }
      // else if target angle is "1 quadrant" away from the wheel Angle spin counterclockwise to the opposite of
      // targetAngle and spin the drive motors in the opposite direction
      else if (targetAngle - wheelAngle <= 180)
      {
        // Distance the wheel must spin is now not the distance between the target and the wheelAngle, but rather
        // the distance between the opposite of the target and the wheelAngle
        error = 180 - (targetAngle - wheelAngle);
        spinDirection = -1;
        driveDirection = -1;
      }
      else if (targetAngle - wheelAngle <= 270)
      {
        error = (targetAngle - wheelAngle) - 180;
        spinDirection = 1;
        driveDirection = -1;
      }
      // if target and wheelAngle are less than 90 degrees apart we should spin directly towards the target angle
      // Here however, we must reverse the spin direction because the target is counterclockwise of the wheelAngle
      else
      {
        error = 360 - (targetAngle - wheelAngle);
        spinDirection = -1;
        driveDirection = 1;
      }
    }
    else if (wheelAngle > targetAngle)
    {
      // The logic below is similar to the logic above, but in the case where wheelAngle > targetAngle
      if (wheelAngle - targetAngle <= 90)
      {
        error = wheelAngle - targetAngle;
        spinDirection = -1;
        driveDirection = 1;
      }
      else if (wheelAngle - targetAngle <= 180)
      {
        error = 180 - (wheelAngle - targetAngle);
        spinDirection = 1;
        driveDirection = -1;
      }
      else if (wheelAngle - targetAngle <= 270)
      {
        error = (wheelAngle - targetAngle) - 180;
        spinDirection = -1;
        driveDirection = -1;
      }
      else
      {
        error = 360 - (wheelAngle - targetAngle);
        spinDirection = 1;
        driveDirection = 1;
      }
    }

    // simple P of PID, makes the wheel move slower as it reaches the target
    double output = WHEEL_SPIN_KP * (error / 90);

    // Move motors at speeds and directions determined earlier
    spinMotor->Set(ControlMode::PercentOutput, output * spinDirection);
    driveMotor->Set(ControlMode::PercentOutput, driveSpeed * driveDirection);
  }

  // Spin swerve module motors to reach the drive speed and spin angle 
  void DriveSwerveModuleMeters(double driveSpeed, double targetRadian)
  {
    DriveSwerveModulePercent(driveSpeed / SWERVE_DRIVE_MAX_MPS, targetRadian);
  }
};

class SwerveDrive
{
private:
  Pigeon2 *pigeonIMU;
  Translation2d m_frontLeft;
  Translation2d m_frontRight;
  Translation2d m_backLeft;
  Translation2d m_backRight;
  SwerveDriveKinematics<4> kinematics;
  SwerveDriveOdometry<4> *odometry;
  Trajectory currentTrajectory;
  frc::ProfiledPIDController<units::meters> xPidContoller;
  frc::ProfiledPIDController<units::meters> yPidContoller;
  frc::ProfiledPIDController<units::centimeter> thetaPidController;

  units::second_t lastOdometryRefresh = Timer::GetFPGATimestamp();

public:
  SwerveModule *FLModule, *FRModule, *BRModule, *BLModule;
  double pigeon_initial;

  // Instantiates SwerveDrive class by creating 4 swerve modules
  SwerveDrive(ctre::phoenix::motorcontrol::can::TalonFX *_FLDriveMotor,
              ctre::phoenix::motorcontrol::can::TalonFX *_FLSpinMotor, frc::DutyCycleEncoder *_FLMagEncoder,
              double _FLEncoderOffset, ctre::phoenix::motorcontrol::can::TalonFX *_FRDriveMotor,
              ctre::phoenix::motorcontrol::can::TalonFX *_FRSpinMotor, frc::DutyCycleEncoder *_FRMagEncoder,
              double _FREncoderOffset, ctre::phoenix::motorcontrol::can::TalonFX *_BRDriveMotor,
              ctre::phoenix::motorcontrol::can::TalonFX *_BRSpinMotor, frc::DutyCycleEncoder *_BRMagEncoder,
              double _BREncoderOffset, ctre::phoenix::motorcontrol::can::TalonFX *_BLDriveMotor,
              ctre::phoenix::motorcontrol::can::TalonFX *_BLSpinMotor, frc::DutyCycleEncoder *_BLMagEncoder,
              double _BLEncoderOffset, Pigeon2 *_pigeonIMU, double robotStartingRadian)
  //TODO CLEAN based off of drive width / length
  : m_frontLeft{0.29845_m, 0.2953_m},
    m_frontRight{0.29845_m, -0.2953_m},
    m_backLeft{-0.29845_m, 0.2953_m},
    m_backRight{-0.29845_m, -0.2953_m},
    kinematics{m_frontLeft, m_frontRight, m_backLeft, m_backRight},
    xPidContoller{X_KP, 0, X_KD,
      frc::TrapezoidProfile<units::meters>::Constraints{AUTO_MAX_MPS, AUTO_MAX_MPS_SQ}},
    yPidContoller{Y_KP, 0, Y_KD,
      frc::TrapezoidProfile<units::meters>::Constraints{AUTO_MAX_MPS, AUTO_MAX_MPS_SQ}},
    thetaPidController{THETA_KP, 0, THETA_KD,
      frc::TrapezoidProfile<units::centimeter>::Constraints{AUTO_MAX_RADPS, AUTO_MAX_RADPS_SQ}}
  {
    FLModule = new SwerveModule(_FLDriveMotor, _FLSpinMotor, _FLMagEncoder, _FLEncoderOffset);
    FRModule = new SwerveModule(_FRDriveMotor, _FRSpinMotor, _FRMagEncoder, _FREncoderOffset);
    BLModule = new SwerveModule(_BLDriveMotor, _BLSpinMotor, _BLMagEncoder, _BLEncoderOffset);
    BRModule = new SwerveModule(_BRDriveMotor, _BRSpinMotor, _BRMagEncoder, _BREncoderOffset);
    
    pigeonIMU = _pigeonIMU;

    thetaPidController.EnableContinuousInput(units::centimeter_t{-1 * M_PI}, units::centimeter_t{M_PI});

    wpi::array<SwerveModulePosition, 4> positions = {FLModule->GetSwerveModulePosition(),
      FRModule->GetSwerveModulePosition(),
      BLModule->GetSwerveModulePosition(),
      BRModule->GetSwerveModulePosition()};

    //will screw up when robot doesn't start at 0 degrees
    odometry = new SwerveDriveOdometry<4>(kinematics, 
    Rotation2d(units::radian_t{GetIMURadians()}), 
    positions, 
    frc::Pose2d(0_m, 0_m, Rotation2d(units::radian_t{robotStartingRadian})));
  }

  double GetIMURadians()
  {
    double pigeon_angle = fmod(pigeonIMU->GetYaw(), 360);
    pigeon_angle -= pigeon_initial;
    if (pigeon_angle < 0)
      pigeon_angle += 360;
    pigeon_angle = 360 - pigeon_angle;
    if (pigeon_angle == 360)
      pigeon_angle = 0;
    pigeon_angle *= M_PI / 180;
    return pigeon_angle;
  }

  void ResetOdometry()
  {
    ResetOdometry(Pose2d(0_m, 0_m, Rotation2d(0_rad)));
  }

  //Resets Odometry
  void ResetOdometry(Pose2d position)
  {
    FLModule->ResetEncoders();
    FRModule->ResetEncoders();
    BLModule->ResetEncoders();
    BRModule->ResetEncoders();

    wpi::array<SwerveModulePosition, 4> positions = {FLModule->GetSwerveModulePosition(),
      FRModule->GetSwerveModulePosition(),
      BLModule->GetSwerveModulePosition(),
      BRModule->GetSwerveModulePosition()};

    odometry->ResetPosition( 
    Rotation2d(units::radian_t{GetIMURadians()}), 
    positions, 
    frc::Pose2d(position));
  }

  void UpdateOdometry()
  {
    SmartDashboard::PutNumber("FL POS", BLModule->GetSwerveModulePosition().distance.value());
    SmartDashboard::PutNumber("FL ANGLE", BLModule->GetSwerveModulePosition().angle.Degrees().value());
    SmartDashboard::PutNumber("ROBOT ANGLE", GetIMURadians());
    wpi::array<SwerveModulePosition, 4> positions = {FLModule->GetSwerveModulePosition(),
      FRModule->GetSwerveModulePosition(),
      BLModule->GetSwerveModulePosition(),
      BRModule->GetSwerveModulePosition()};
    odometry->Update(units::radian_t{GetIMURadians()}, positions);
  }

  Pose2d GetPose()
  {
    return odometry->GetPose();
  }

  void DriveSwervePercent(double FWD_Drive_Speed, double STRAFE_Drive_Speed, double Turn_Speed)
  {
    // If there is no drive input, don't drive the robot and just end the function
    if (FWD_Drive_Speed == 0 && STRAFE_Drive_Speed == 0 && Turn_Speed == 0)
    {
      FLModule->StopSwerveModule();
      FRModule->StopSwerveModule();
      BLModule->StopSwerveModule();
      BRModule->StopSwerveModule();

      return;
    }

    // Determine wheel speeds / wheel target positions
    // Equations explained at:
    // https://www.chiefdelphi.com/t/paper-4-wheel-independent-drive-independent-steering-swerve/107383
    // After clicking above link press the top download to see how the equations work
    double driveRadius = sqrt(pow(DRIVE_LENGTH, 2) + pow(DRIVE_WIDTH, 2));

    double A = STRAFE_Drive_Speed - Turn_Speed * (DRIVE_LENGTH / driveRadius);
    double B = STRAFE_Drive_Speed + Turn_Speed * (DRIVE_LENGTH / driveRadius);
    double C = FWD_Drive_Speed - Turn_Speed * (DRIVE_WIDTH / driveRadius);
    double D = FWD_Drive_Speed + Turn_Speed * (DRIVE_WIDTH / driveRadius);

    double FR_Target_Angle = atan2(B, C) * 180 / M_PI;
    double FL_Target_Angle = atan2(B, D) * 180 / M_PI;
    double BL_Target_Angle = atan2(A, D) * 180 / M_PI;
    double BR_Target_Angle = atan2(A, C) * 180 / M_PI;

    double FR_Drive_Speed = sqrt(pow(B, 2) + pow(C, 2));
    double FL_Drive_Speed = sqrt(pow(B, 2) + pow(D, 2));
    double BL_Drive_Speed = sqrt(pow(A, 2) + pow(D, 2));
    double BR_Drive_Speed = sqrt(pow(A, 2) + pow(C, 2));

    // If Turn Speed and Drive Speed are both high, the equations above will output a number greater than 1.
    // Below we must scale down all of the drive speeds to make sure we do not tell the motor to go faster
    // than it's max value.

    double max = FR_Drive_Speed;
    if (FL_Drive_Speed > max)
      max = FL_Drive_Speed;
    if (BL_Drive_Speed > max)
      max = BL_Drive_Speed;
    if (BR_Drive_Speed > max)
      max = BR_Drive_Speed;

    if (max > 1)
    {
      FL_Drive_Speed /= max;
      FR_Drive_Speed /= max;
      BL_Drive_Speed /= max;
      BR_Drive_Speed /= max;
    }

    frc::SmartDashboard::PutNumber("FR Drive Speed", FR_Drive_Speed);
    frc::SmartDashboard::PutNumber("FR Target Angle", FR_Target_Angle);

    // Make all the motors move
    FLModule->DriveSwerveModulePercent(FL_Drive_Speed, FL_Target_Angle);
    FRModule->DriveSwerveModulePercent(FR_Drive_Speed, FR_Target_Angle);
    BLModule->DriveSwerveModulePercent(BL_Drive_Speed, BL_Target_Angle);
    BRModule->DriveSwerveModulePercent(BR_Drive_Speed, BR_Target_Angle);
  }

  void DriveSwerveMetersAndRadiansFieldOriented(double FWD_Drive_Speed, double STRAFE_Drive_Speed, double Turn_Speed)
  {
    // Use pigion_angle to determine what our target movement vector is in relation to the robot
    // This code keeps the driving "field oriented" by determining the angle in relation to the front of the robot we
    // really want to move towards
    double pigeon_angle = GetIMURadians();
    FWD_Drive_Speed = FWD_Drive_Speed * cos(pigeon_angle) + STRAFE_Drive_Speed * sin(pigeon_angle);
    STRAFE_Drive_Speed = -1 * FWD_Drive_Speed * sin(pigeon_angle) + STRAFE_Drive_Speed * cos(pigeon_angle);
    DriveSwervePercent(FWD_Drive_Speed / SWERVE_DRIVE_MAX_MPS, STRAFE_Drive_Speed / SWERVE_DRIVE_MAX_MPS, Turn_Speed / MAX_RADIAN_PER_SECOND);
  }

  void SetModuleStates(std::array<SwerveModuleState, 4> states)
  { 
    FRModule->DriveSwerveModuleMeters(states[0].speed.value(), states[0].angle.Degrees().value());
    FLModule->DriveSwerveModuleMeters(states[1].speed.value(), states[1].angle.Degrees().value());
    BRModule->DriveSwerveModuleMeters(states[2].speed.value(), states[2].angle.Degrees().value());
    BLModule->DriveSwerveModuleMeters(states[3].speed.value(), states[3].angle.Degrees().value());
  }

  void SetDriveToPoseOdometry(Pose2d target)
  {
    xPidContoller.SetGoal(target.X());
    yPidContoller.SetGoal(target.Y());
    thetaPidController.SetGoal(units::centimeter_t{target.Rotation().Radians().value()});
  }

  void DriveToPoseOdometry(Pose2d target)
  {
    double x = xPidContoller.Calculate(odometry->GetPose().X(), target.X());
    double y = yPidContoller.Calculate(odometry->GetPose().Y(), target.Y());
    double theta = thetaPidController.Calculate(units::centimeter_t{odometry->GetPose().Rotation().Radians().value()}, units::centimeter_t{target.Rotation().Radians().value()});
    SmartDashboard::PutNumber("Drive To X", x);
    SmartDashboard::PutNumber("Drive To Y", y);
    SmartDashboard::PutNumber("Drive To Theta", theta);

    DriveSwerveMetersAndRadiansFieldOriented(x, y, theta);
  }

  void DriveToPoseVision(Pose2d target)
  {
    //this code is weird because it assumes the robot is at 0,0 and the target is some distance away from that as described by the vision tracking
    double x = xPidContoller.Calculate(0_m, target.X());
    double y = yPidContoller.Calculate(0_m, target.Y());
    double theta = thetaPidController.Calculate(0_cm, units::centimeter_t{target.Rotation().Radians().value()});
    SmartDashboard::PutNumber("Drive To X", x);
    SmartDashboard::PutNumber("Drive To Y", y);
    SmartDashboard::PutNumber("Drive To Theta", theta);

    DriveSwerveMetersAndRadiansFieldOriented(x, y, theta);
  }

  void DriveToPoseCombo(Pose2d visionInput, Pose2d target, double refreshTime)
  {
    //This code is weird because it assumes the feducial is at 0m, 0m (x, y)
    if (refreshTime < (Timer::GetFPGATimestamp() - lastOdometryRefresh).value())
    {
      //line below may not work idk lmao
      ResetOdometry(visionInput.operator*(-1));
      lastOdometryRefresh = Timer::GetFPGATimestamp();
    }

    DriveToPoseOdometry(target);
  }

  void TurnToPointWhileDriving(double fwdSpeed, double strafeSpeed, Translation2d point)
  {
    Translation2d diff = point - GetPose().Translation();
    double targetAngle = atan2(diff.Y().value(), diff.X().value());
    double theta = thetaPidController.Calculate(units::centimeter_t{GetPose().Rotation().Radians().value()}, units::centimeter_t{targetAngle});
    DriveSwervePercent(fwdSpeed, strafeSpeed, theta / MAX_RADIAN_PER_SECOND);
  }

  void GenerateTrajecotory(vector<Translation2d> waypoints, Pose2d goal)
  {
    //unfinished
    TrajectoryConfig trajectoryConfig{units::meters_per_second_t{SWERVE_DRIVE_MAX_MPS}, units::meters_per_second_squared_t{SWERVE_DRIVE_MAX_ACCELERATION}};
    trajectoryConfig.SetKinematics(kinematics);
    TrajectoryGenerator trajectoryGenerator{};

    //May delete itself and break everything
    currentTrajectory = trajectoryGenerator.GenerateTrajectory(
      this->GetPose(),
      waypoints,
      goal,
      trajectoryConfig
    );
  }
};
