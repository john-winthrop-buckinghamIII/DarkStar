//example code we made to change the accel range from +-2g to +-16g on pololu IMU.

#include <Wire.h>
#include <LSM6.h>

LSM6 imu;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();

  if (!imu.init()) {
    Serial.println("this ain't working");
  imu.enableDefault();
  imu.writeReg(LSM6::CTRL3_C, 0x44);
  }

   // Here is how I set up the 16G max instead of the factory default.
   // Sample Rate = 1.66 kHz (0110)
   // Accel = +-16G (01)
   // BW = wtv the board comes with (00)
   // adds all up to 0110 01 00, convert ts to hex lil bro
  imu.writeReg(LSM6::CTRL1_XL, 0x64);
}

void loop() {
  imu.read();
  // this number is from the data sheet, basically it is the conversion factor that combines the sensitivity of this mg range (0.488mg/LSB) and then converts it to G (0.001).
  // = (0.488mg/LSB) * (0.001g)
  float ax = imu.a.x * 0.000488f;
  float ay = imu.a.y * 0.000488f;
  float az = imu.a.z * 0.000488f;

  Serial.print("accel: ");
  Serial.print(ax, 4); Serial.print(", ");
  Serial.print(ay, 4); Serial.print(", ");
  Serial.println(az, 4);

  delay(100);
}
