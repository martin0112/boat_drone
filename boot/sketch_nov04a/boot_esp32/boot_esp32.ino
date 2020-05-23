                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     #define MOSFET_PIN 25
#define SWITCH_PIN 36
#define SDA 21
#define SCL 22
#define RX1 13
#define TX1 10
#define RX2 14
#define TX2 12
#define send_every 200


#define R_0 33
#define R_1 32 
#define M_0 19 
#define M_1 26







#include <U8g2lib.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <MPU9250_asukiaaa.h>
#include <I2CEEPROM.h>
//Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

TinyGPSPlus gps;
double lati, lon, velocity, heading, no_sat, lat_home, lng_home;

MPU9250 mySensor;
float tilt, tilt_gyro, tilt_accel;
float roll, roll_gyro, roll_accel;
float total_accel;
float T_accel = 0.3;
bool fix;

//Motordad
#define v_motor_max 100
#define u_rudder_max 200

//Flugschreiber
#define CHIP_ADDRESS 0x50 // Address of EEPROM chip (24LC256->0x50)
#define EEPROM_BYTES 32768 // Number of bytes in EEPROM chip
#define t_cycle_recorder 0.1
#define amount_data 9
int last_record = 0;
int16_t no_records = 0;
String betrieb = "nichts";
I2CEEPROM i2c_eeprom(CHIP_ADDRESS); // Create I2C EEPROM instance
unsigned int current_address = 0;

//Autopilot
float wp_lng[10] = {7.5411, 0, 0, 0, 0, 0, 0, 0, 0, 0};
float wp_lat[10] = {51.5316, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int wp_act = 0;
float heading_sp = 0;
float distance = 0;
char autopilot_mode = 'M';
unsigned long last_received;
int home_after_not_received = 30000; //nach 1000 ms Home modus wenn kein empfang
float kp = 1;
float ki = 0;
float td = 0;
float int_error;
float last_error;
int cv_rudder;
int error;
bool home_set = false;

//switch
float switch_state_float;
bool switch_state;
float T_switch = 1; //nach s an

//cycle time
unsigned long previos_micros;
int n_loop;
int loop_average = 10;
float loop_time;

//telemetry
unsigned long last_send;

//MOtor Rudder
int freq = 1000;
int resolution = 8;
int dutyCycle = 0;

//Radio
char commands[] = { 'N', 'N', 'N', 'N', 'N'};

void setup() {
  Wire.begin(SDA, SCL);

  //Motor

  ledcSetup(0, freq, resolution); //Ruder
  ledcSetup(1, freq, resolution); //Ruder
  ledcSetup(2, freq, resolution); //Motor
  ledcSetup(3, freq, resolution); //Motor
  ledcAttachPin(R_0, 0);
  ledcAttachPin(R_1, 1);
  ledcAttachPin(M_0, 2);
  ledcAttachPin(M_1, 3);

  //Dpsiplay
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font

  Serial.begin(115200);
  Serial2.println("Start Boot");
  //GPS
  Serial1.begin(115200, SERIAL_8N1, RX1, TX1);

  //MOSFET
  pinMode(  MOSFET_PIN, OUTPUT);

  //SWITCH
  pinMode(  SWITCH_PIN, INPUT);


  //Radio Zigbee
  Serial2.begin(9600, SERIAL_8N1, RX2, TX2);

  //MPU9250
  mySensor.setWire(&Wire);
  mySensor.beginAccel();
  mySensor.beginGyro();
  mySensor.beginMag();

  //cycle time
  previos_micros = millis();

  digitalWrite(MOSFET_PIN, LOW);
  delay(1000);
  digitalWrite(MOSFET_PIN, HIGH);
  delay(1000);
  digitalWrite(MOSFET_PIN, LOW);
  delay(1000);
  digitalWrite(MOSFET_PIN, HIGH);
  delay(1000);
  digitalWrite(MOSFET_PIN, LOW);
  delay(1000);
  digitalWrite(MOSFET_PIN, HIGH);
  delay(1000);
  digitalWrite(MOSFET_PIN, LOW);
  delay(1000);
  digitalWrite(MOSFET_PIN, HIGH);
  delay(1000);
}

void loop() {
  read_radio();
  get_GPS();
  //write_display();
  send_tele();
  read_switch();
  set_home();
  autopilot();
  //check_emergency();
  flugschreiber();
  loop_time_measurement();

  // transfer internal memory to the display
  delay(10);

}

void check_emergency() //Falls kein Funksignal
{
  if ((millis() - last_received) > home_after_not_received)
  {
    autopilot_mode = 'C';
    heading_sp = gps.courseTo((double)lati, (double)lon, lat_home, (double)lng_home);

  }
  


}

void loop_time_measurement()
{

  n_loop++;

  if (n_loop == loop_average)
  {
    n_loop = 0;
    loop_time = (millis() - previos_micros) / loop_average / 1000.0;
    //Serial.println(loop_time);
    previos_micros = millis();


  }



}


//Switch on board
void read_switch()
{
 /* Manual order
  ONNNNNX
  Flight recorder 
  RW recording data
  RR sending recorded data
  RN flight recorder idle
  Set new home location
  Autopilot
  HH return home
  HS set home
 */ 
  int in = digitalRead(SWITCH_PIN);


  //switch dampening
  switch_state_float = switch_state_float + (in - switch_state_float) / T_switch * loop_time;
  switch_state = false;
  if (switch_state_float > 0.9)
  {
    switch_state = true;


  }


}


void read_radio()
{
  while (Serial2.available() > 0)
  {
    char first_byte=Serial2.read();

    
    

    //Get manual commands
    if (first_byte == 'O')
    {
      char commands_temp[5];
      commands_temp[0] = Serial2.read();
      commands_temp[1] = Serial2.read();
      commands_temp[2] = Serial2.read();
      commands_temp[3] = Serial2.read();
      commands_temp[4] = Serial2.read();

      if (Serial2.read() == 'X')
      {
        strncpy(commands, commands_temp, 5);
        autopilot_mode = 'M';
        last_received = millis();

      }


    }

    //flight recorder
    if (first_byte == 'R')
    {
      char second_byte=Serial2.read();
      
        

      if (second_byte == 'W')
      {
        betrieb = "schreiben";

        last_received = millis();

      }

      if (second_byte == 'R')
      {
        betrieb = "lesen";
        last_received = millis();

      }
      if (second_byte == 'N')
      {
        betrieb = "nichts";
        last_received = millis();

      }


    }

   //New configuration
    if (first_byte == 'C')
    {
       char second_byte=Serial2.read();
       //Configure Kp of PnID
    if (second_byte == 'P')
      {
          char PID_KP_char[4];
          char sign=Serial2.read();          
          PID_KP_char[0]=Serial2.read();
          PID_KP_char[1]=Serial2.read();
          PID_KP_char[2]=Serial2.read();
          PID_KP_char[3]=Serial2.read();

          
          if (isDigit(PID_KP_char[0])&&isDigit(PID_KP_char[1])&&isDigit(PID_KP_char[2])&&isDigit(PID_KP_char[3]))
          {
          if (sign=='-')
          {
          kp=-atoi(PID_KP_char)/10;
          }
          if (sign=='+')
          {
          kp=atoi(PID_KP_char)/10;
          }
            

          }

          Serial2.print("Kp set:");
          Serial2.println(kp);
          

        
      }

      
    }


   //Set new home command
    if (first_byte == 'H')
    {
      char second_byte=Serial2.read();


      if (second_byte == 'S')
      {
        home_set=false;
        
      }

      if (second_byte == 'H')
      {
        autopilot_mode = 'C';
        heading_sp = gps.courseTo((double)lati, (double)lon, lat_home, (double)lng_home);
        Serial2.print("SReturnHomeX");
        last_received = millis();
        
      }
      if (second_byte == 'C')
      {  
          char heading_sp_char[3];
          heading_sp_char[0]=Serial2.read();
          heading_sp_char[1]=Serial2.read();
          heading_sp_char[2]=Serial2.read();
          if (isDigit(heading_sp_char[0])&&isDigit(heading_sp_char[1])&&isDigit(heading_sp_char[2]))
          {
          heading_sp=atoi(heading_sp_char);

          autopilot_mode = 'C';

          Serial2.print("Heading_sp: ");          
          Serial2.print(heading_sp);          
          }
            
      }



    }
    




  }

}

void send_tele()
{

  if ((millis() - last_send) > send_every)
  {
    int lati_digits = (lati - int(lati)) * 100000;
    int longi_digits = (lon - int(lon)) * 100000;
    int heading_digits = round(heading);


    String temp_radio = "T" + String(lati_digits) + "L" + String(longi_digits) + "S" + String(velocity, 2) + "H" + String(heading_digits) + "E";


    Serial2.println(temp_radio);
    //Serial.println(temp_radio);

    last_send = millis();
  }
}

void write_display()
{
  u8g2.clearBuffer();         // clear the internal memory
  String temp_orient = "Roll:" + String(roll) + " Tilt:" + String(tilt);
  String temp_gps_0 = "No FIX " + String(no_sat, 0);
  String temp_gps_1;
  String temp_gps_2;

  if (gps.location.isValid())
  {


    temp_gps_0 = "Lat:" + String(lati, 4) + " Lon:" + String(lon, 4);
    temp_gps_1 = "Speed:" + String(velocity, 2) + " Heading" + String(heading, 0);
    temp_gps_2 = "No Sat:" + String(no_sat, 0);

  }

  u8g2.drawStr(0, 10, temp_orient.c_str()); // write something to the internal memory
  u8g2.drawStr(0, 25, temp_gps_0.c_str()); // write something to the internal memory
  u8g2.drawStr(0, 40, temp_gps_1.c_str()); // write something to the internal memory
  u8g2.drawStr(0, 55, temp_gps_2.c_str()); // write something to the internal memory

  u8g2.sendBuffer();

}


void get_orienation()
{
  mySensor.accelUpdate();
  float accelX = mySensor.accelX();
  float accelY = mySensor.accelY();
  float accelZ = mySensor.accelZ();

  mySensor.gyroUpdate();
  float gX = mySensor.gyroX() + 1.5;
  float gY = mySensor.gyroY() + 0.5;
  float gZ = mySensor.gyroZ();




  float tilt_accel_now = accelZ > 0 ? -180 - atan2 (accelY , ( sqrt ((accelX * accelX) + (accelZ * accelZ)))) / 1.5 * 90 : atan2 (accelY , ( sqrt ((accelX * accelX) + (accelZ * accelZ)))) / 1.5 * 90;
  tilt_accel_now = tilt_accel_now < -180 ? 360 + tilt_accel_now : tilt_accel_now;
  float roll_accel_now = accelZ > 0 ? 180 - atan2(-accelX , ( sqrt((accelY * accelY) + (accelZ * accelZ)))) / 1.5 * 90 : atan2(-accelX , ( sqrt((accelY * accelY) + (accelZ * accelZ)))) / 1.5 * 90;
  roll_accel_now = roll_accel_now > 180 ? roll_accel_now - 360 : roll_accel_now;
  float accel_now =  sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);

  total_accel = total_accel + (accel_now - total_accel) / T_accel * loop_time;


  tilt_accel = (1 - T_accel) * tilt_accel_now + T_accel * tilt_accel;
  roll_accel = (1 - T_accel) * roll_accel_now + T_accel * roll_accel;
  /*
    Serial.print(accelX );
    Serial.print(";");
    Serial.print(accelY );
    Serial.print(";");
    Serial.print(accelZ);
    Serial.print(";");

    Serial.println(roll_accel);
  */
  tilt = 0.98 * (tilt + gX * loop_time) + 0.02 * tilt_accel;
  roll = 0.98 * (roll + gY * loop_time) + 0.02 * roll_accel;


}


void get_GPS()
{

  while (Serial1.available() > 0)
  {
    if (gps.encode(Serial1.read()))
    {



      if (gps.location.isValid())
      {


        //Serial.println("FIX");

        lati = gps.location.lat();
        lon = gps.location.lng();
        heading = gps.course.deg();
        velocity = gps.speed.mps();
        no_sat = gps.satellites.value();
        fix = true;


      }
      else
      {
        //Serial.println("No FIX");
        //Serial.println("Char from GPS: "+String(gps.charsProcessed()));
        no_sat = gps.satellites.value();
        fix = false;
        //Serial.println(String(no_sat,0));
        lati = 0;
        lon = 0;
        heading = 0;
        velocity = 0;

      }
    }


  }
}

void set_home()
{
  if ((switch_state && gps.location.isValid()) || (home_set == false && gps.location.isValid()))
  {
    lat_home = lati;
    lng_home = lon;
    int lati_digits = (lati - int(lati)) * 100000;
    int longi_digits = (lon - int(lon)) * 100000;


    String temp_radio = "H" + String(lati_digits) + "L" + String(longi_digits) + "E";
    
    Serial2.println(temp_radio);

    home_set = true;

    switch_state_float = 0;
  }


}

void autopilot()
{
  
  if (autopilot_mode == 'M')
  {
    //Rudder
    if (commands[1] == 'L')
    {
      ledcWrite(0, u_rudder_max);
      ledcWrite(1, 0);


    }


    if (commands[1] == 'R')
    {
      ledcWrite(1, u_rudder_max);
      ledcWrite(0, 0);
    }

    if (commands[1] == 'N')
    {
      ledcWrite(1, 0);
      ledcWrite(0, 0);
    }
    //Motor
    int speed_percent = 0;
    int vorwarts = 0;
    int rueckwaerts = 0;

    int schnell = 0;
    int langsam = 0;


    if (commands[0] == 'V')
    {
      vorwarts = 1;
      schnell = 0;
      langsam = 0;
    }

    if (commands[0] == 'N')
    {
      vorwarts = 0;
      schnell = 0;
      langsam = 0;



    }

    if (commands[0] == 'Z')
    {
      vorwarts = -1;
      schnell = 0;
      langsam = 0;

    }



    if (commands[2] == 'S')
    {

      schnell = 1;
      langsam = 0;

    }

    if (commands[2] == 'L')
    {

      schnell = 0;
      langsam = 1;

    }



    speed_percent = vorwarts * (50 + schnell * 50 - langsam * 25) ;


    int geschwindigkeit = abs((int)(speed_percent / 100.0 * v_motor_max));


    if (speed_percent > 0)
    {
      ledcWrite(2, geschwindigkeit);
      ledcWrite(3, 0);

    }
    if (speed_percent < 0)
    {
      
      ledcWrite(3, geschwindigkeit);
      ledcWrite(2, 0);
         
    }
    if (speed_percent == 0)
    {
      ledcWrite(2, 0);
      ledcWrite(3, 0);
    }


  }

  

  if (autopilot_mode == 'C'&&gps.location.isValid())
  {
    
    error =(int)(heading_sp - heading);
    if (error > 180)
    {
      error = error - 360;
    }

    else if (error < -180)
    {
      error = 360 + error;
    }
    float derror_dt = (error - last_error) / loop_time;
    int_error = int_error + error * loop_time;

    last_error = error;

    cv_rudder =(int)(kp * error + ki * int_error + td * derror_dt) ;
    int u_cv_rudder=cv_rudder<u_rudder_max?cv_rudder:u_rudder_max;
    Serial2.print("Data;");
    Serial2.print(heading_sp);
    Serial2.print(";");
    Serial2.print(heading);
    Serial2.print(";");
    Serial2.print(error);
    Serial2.print(";");
    Serial2.print(cv_rudder);
    Serial2.print(";");
    Serial2.println(u_cv_rudder);
   
    
    //Rudder
    if (cv_rudder>=0)
    {
      ledcWrite(0, u_cv_rudder);
      ledcWrite(1, 0);


    }
      if (cv_rudder<0)
    {
      ledcWrite(1, u_cv_rudder);
      ledcWrite(0, 0);


    }

    float speed_percent = 50.0;
    int geschwindigkeit = abs((int)(speed_percent / 100.0 * v_motor_max));
    ledcWrite(2, geschwindigkeit);
    ledcWrite(3, 0);




  }



}

void flugschreiber()
{



  if (betrieb == "lesen")
  {

  Serial2.println("Reading data");


    byte big_byte = i2c_eeprom.read(0);
    byte small_byte = i2c_eeprom.read(1);
    int16_t number_records = int16_t(big_byte << 8) + int16_t(small_byte);
  Serial2.print("No Records");
  Serial2.println(number_records);

  


    for (int i = 0; i < number_records; i++)
    {
      for (int k = 0; k < amount_data; k++)
      {
        byte big_byte = i2c_eeprom.read(amount_data * i * 2 + k * 2 + 2);
        byte small_byte = i2c_eeprom.read(amount_data * i * 2 + k * 2 + 1 + 2);
        int16_t recorded_data = int16_t(big_byte << 8) + int16_t(small_byte);
        Serial2.print(recorded_data);
        Serial2.print(";");



      }
      Serial2.println();
      

    }

    betrieb="nichts";
    Serial2.println("Export finished");




  }





  if (betrieb == "schreiben")
  {

    int time_since_record = millis() - last_record;
    if (time_since_record > t_cycle_recorder * 1000)
    {
      

      byte payload[amount_data * 2];


      payload[0] = highByte(int16_t (time_since_record));
      payload[1] = lowByte(int16_t (time_since_record));

      payload[2] = highByte(int16_t (heading_sp));
      payload[3] = lowByte(int16_t (heading_sp));

      payload[4] = highByte(int16_t (heading));
      payload[5] = lowByte(int16_t (heading));

      payload[6] = highByte(int16_t (velocity));
      payload[7] = lowByte(int16_t (velocity));

      payload[8] = highByte(int16_t (roll));
      payload[9] = lowByte(int16_t (roll));


      payload[10] = highByte(int16_t (tilt));
      payload[11] = lowByte(int16_t (tilt));

      payload[12] = highByte(int16_t (cv_rudder));
      payload[13] = lowByte(int16_t (cv_rudder));

      payload[14] = highByte(int16_t (error));
      payload[15] = lowByte(int16_t (error));


      payload[16] = highByte(int16_t (int_error));
      payload[17] = lowByte(int16_t (int_error));


      if ((no_records * amount_data * 2 + 2) > EEPROM_BYTES)
      {
        no_records = 0;
        current_address = 0;

      }
      

      for (int i = 0; i < amount_data * 2; i++)
      {


        i2c_eeprom.write(current_address + 2, payload[i]);
        current_address++;


      }
      no_records++;
      i2c_eeprom.write(0, highByte(no_records));
      i2c_eeprom.write(1, lowByte(no_records));
     
      




      last_record = millis();
    }

  }


}
