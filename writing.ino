// Thingspeak
#include <SoftwareSerial.h>
SoftwareSerial EspSerial(6, 7); // Rx,  Tx
#include <stdlib.h>
#define HARDWARE_RESET 8
#define FREEZE_LED 13
#define lpg_gas_sensor 10
#define servomotor 11
#define alarm 12
boolean lpg_sensor_status;
int spare;
long writeTimingSeconds = 17;
long readTimingSeconds = 10;
long startReadTiming = 0;
long elapsedReadTime = 0;
long startWriteTiming = 0;
long elapsedWriteTime = 0;
boolean knob;
boolean Alarm;
boolean error;
void setup()
{
  Serial.begin(9600);
  pinMode(HARDWARE_RESET,OUTPUT);
  pinMode(lpg_gas_sensor,INPUT);
  pinMode(servomotor,OUTPUT);
  pinMode(FREEZE_LED,OUTPUT);
  pinMode(alarm,OUTPUT);
  digitalWrite(HARDWARE_RESET, HIGH);
  digitalWrite(FREEZE_LED, LOW);
  EspSerial.begin(9600);
  EspHardwareReset();
  startReadTiming = millis();
  startWriteTiming = millis();
}
void loop()
{
  start:
  error=0;
  elapsedWriteTime = millis()-startWriteTiming;
  elapsedReadTime = millis()-startReadTiming;
  if (elapsedWriteTime >(writeTimingSeconds*1000))
  {
    ESPcheck();
    readSensors();
    writeWebsiteData("sensor_write",lpg_sensor_status);
    startWriteTiming = millis();
  }
  if (error==1)
  {
    Serial.println(" <<<< ERROR >>>>\n");
    digitalWrite(FREEZE_LED, HIGH);
    delay (2000);
    goto start;
  }
  if (elapsedReadTime >(readTimingSeconds*1000))
  {
    ESPcheck();
    int command = readWebsiteData("knob",knob);
    Serial.print(command);
    if(command != 9){
      knob = command;
    }
    delay (1000);
    command = readWebsiteData("alarm",Alarm);
    Serial.print(command+"\n");
    if (command != 9){
      Alarm = command;
    }
    takeActions();
    writeWebsiteData("knob_write",knob);
    writeWebsiteData("alarm_write",Alarm);
    startReadTiming = millis();
  }
}
void readSensors(void)
{
  lpg_sensor_status=digitalRead(lpg_gas_sensor);
  Serial.println("Reading Sensor Data:##START");
  Serial.println(lpg_sensor_status);
  Serial.println("Reading Sensor Data:##END");
}
void writeWebsiteData(String write,boolean device)
{
  startThingSpeakCmd();
  String status;
  if(device==1){
    status="on";
  }
  else{
    status="off";
  }
  Serial.println("Sending Sensor Data.....");
  String getStr = "GET /"+write+".php?status="+status+" HTTP/1.1\r\nHost: api.virtualworld.today\r\nConnection: keep-alive\r\n\r\n";
  Serial.println(getStr);
  sendWebsiteGetCmd(getStr);
}
void EspHardwareReset(void)
{
  Serial.println("Reseting.......");
  digitalWrite(HARDWARE_RESET, LOW);
  delay(500);
  digitalWrite(HARDWARE_RESET, HIGH);
  delay(8000);
  Serial.println("RESET");
}
void startThingSpeakCmd(void)
{
  EspSerial.flush();
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += "api.virtualworld.today";
  cmd += "\",80";
  EspSerial.println(cmd);
  Serial.print("Start cmd: ");
  Serial.println(cmd);
  if(EspSerial.find("Error"))
  {
    Serial.println("AT+CIPSTART error");
    return;
  }
}
String sendWebsiteGetCmd(String getStr)
{
  String cmd = "AT+CIPSEND=";
  cmd += String(getStr.length());
  EspSerial.println(cmd);
  Serial.print("lenght cmd: ");
  Serial.println(cmd);
  if(EspSerial.find(">"))
  {
    EspSerial.print(getStr);
    Serial.print("REQUEST==>: ");
    Serial.println(getStr);
    delay(500);
    String messageBody = "";
    while (EspSerial.available())
    {
      String line = EspSerial.readStringUntil('\n');
      if (line.length() == 1)
      { 
        messageBody = EspSerial.readStringUntil('\n');
      }
    }
    Serial.print("MessageBody received: ");
    Serial.println(messageBody);
    return messageBody;
  }
  else
  {
    EspSerial.println("AT+CIPCLOSE");
    Serial.println("ESP8266 CIPSEND ERROR: RESENDING");
    spare = spare + 1;
    error=1;
    return "error";
  }
}
int readWebsiteData(String device_name,boolean device_status)
{
  startThingSpeakCmd();
  int command;
  String getStr = "GET /action_read.php?on"+device_name+" HTTP/1.1\r\nHost: api.virtualworld.today\r\nConnection: keep-alive\r\n\r\n";
  String messageDown = sendWebsiteGetCmd(getStr);
  Serial.print("Sending ...: ");
  Serial.print("Receive Action status:..,............ ");
  Serial.println(messageDown);
  if(messageDown.indexOf("{STATUS[NOT TAKEN]STATUS SEND}")>=0 && messageDown.indexOf("{ACTION[off]ACTIONEND}")>=0){
    Serial.println("##\nTaking...Action.."+device_name+" off\n###\n");
    Serial.print(device_name+"Action received .. ...: ");
    device_status=1;
  }
  else{
    device_status=1;
  }
  if (messageDown[5] == 49)
  {
    command = messageDown[7]-48;
    Serial.print("Command received: ");
    Serial.println(command);
  }
  else command = 9;
  return command;
}
void takeActions(void)
{
  Serial.print("lpg_sensor_status is ");
  Serial.println(lpg_sensor_status);
  Serial.print("knob: ");
  Serial.println(knob);
  Serial.print("alarm: ");
  Serial.println(Alarm);
  if (knob == 1) digitalWrite(servomotor, HIGH);
  else digitalWrite(servomotor, LOW);
  Serial.print("knob action performed");
  if (Alarm == 1) digitalWrite(alarm, HIGH);
  else digitalWrite(alarm,LOW);
  Serial.print("alarm action performed");
}
boolean ESPcheck(void)
{
  EspSerial.println("AT");

  if (echoFind("OK"))
  {
    digitalWrite(FREEZE_LED, LOW);
    return true;
  }
  else 
  {
    Serial.println("ESP Freeze *********************************************************************");
    digitalWrite(FREEZE_LED, HIGH);
    EspHardwareReset();
    return false;
  }
}
boolean echoFind(String keyword)
{
 byte current_char = 0;
 byte keyword_length = keyword.length();
 long deadline = millis() + 5000;
 while(millis() < deadline){
  if (EspSerial.available()){
    char ch = EspSerial.read();
    Serial.write(ch);
    if (ch == keyword[current_char])
      if (++current_char == keyword_length){
       Serial.println();
    }
   }
  }
 return false;
}
