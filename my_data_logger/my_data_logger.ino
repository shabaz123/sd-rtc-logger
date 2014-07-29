/**************************************************************
 * Data logger
 * Based on code in the public domain
 * created  24 Nov 2010
 * modified 9 Apr 2012 by Tom Igoe
 *
 * shabaz July 2014: Adapted for TIVA EK-TM4C123GXL
 * shabaz July 2014: RTC code added 
 **************************************************************/

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <stdarg.h>

#define PUSH1_PRESSED digitalRead(PUSH1)==LOW
#define PUSH1_UNPRESSED digitalRead(PUSH1)==HIGH

#define PUSH2_PRESSED digitalRead(PUSH2)==LOW
#define PUSH2_UNPRESSED digitalRead(PUSH2)==HIGH

#define PB2_ISHIGH digitalRead(PB_2)==HIGH
#define PB2_ISLOW digitalRead(PB_2)==LOW

#define SEC 0
#define MIN 1
#define HR 2
#define DAY 3
#define DATE 4
#define MONTH 5
#define YEAR 6


const int chipSelect = 8;
const char days[][4]={"XXX", "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
File dataFile;

void serial_printf(const char *fmt, ... ){
        char tmp[64]; // resulting string limited to 64 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(tmp, 64, fmt, args);
        va_end (args);
        Serial.print(tmp);
}

void
printmenu(void)
{
  serial_printf("Hello\n\r");
  Serial.println("Commands:");
  Serial.println("time (displays the current time)");
  Serial.println("hr xx (00-23)");
  Serial.println("min xx (00-59)");
  Serial.println("sec xx (00-59)");
  Serial.println("day x (1=SUN, 2=MON, 3=TUE, 4=WED, 5=THU, 6=FRI, 7=SAT)");
  Serial.println("date xx (01-31)");
  Serial.println("month xx (01-12)");
  Serial.println("year xxxx (2014-2099)");
}

void
get_rtc(char* d)
{
  int i;
  Wire.beginTransmission(0x68);
  Wire.write(0x00); // index 0
  Wire.endTransmission();
  Wire.requestFrom(0x68, 7); // request 7 bytes
  if(Wire.available())
  { 
    for (i=0; i<7; i++)
    {
      d[i]=Wire.read();
    }
  }
}

float
get_temp(void)
{
  float temp;
  char th, tl;
  Wire.beginTransmission(0x68);
  Wire.write(0x11); // index 0x11
  Wire.endTransmission();
  Wire.requestFrom(0x68, 2); // request 2 bytes
  if(Wire.available())
  {
    th=Wire.read();
    tl=Wire.read();
  }
  temp=((float)(th & 0x7f)) + ((float)(tl>>6))*0.25;
  return(temp);
}

void
print_date(char* d)
{
  serial_printf("%s %d/%d/20%02d",
      days[d[DAY] & 0x07],
      ((d[DATE] & 0x30)>>4)*10 + (d[DATE] & 0x0f),
      ((d[MONTH] & 0x10)>>4)*10 + (d[MONTH] & 0x0f),
      ((d[YEAR] & 0xf0)>>4)*10 + (d[YEAR] & 0x0f) );
}

void
print_time(char* d)
{
  serial_printf("%02d:%02d:%02d",
      ((d[HR] & 0x30)>>4)*10 + (d[HR] & 0x0f),
      ((d[MIN] & 0xf0)>>4)*10 + (d[MIN] & 0x0f),
      ((d[SEC] & 0xf0)>>4)*10 + (d[SEC] & 0x0f) );
}

void sprint_date(char* buf, char* d)
{
  sprintf(buf, "%s %d/%d/20%02d",
      days[d[DAY] & 0x07],
      ((d[DATE] & 0x30)>>4)*10 + (d[DATE] & 0x0f),
      ((d[MONTH] & 0x10)>>4)*10 + (d[MONTH] & 0x0f),
      ((d[YEAR] & 0xf0)>>4)*10 + (d[YEAR] & 0x0f) );
}

void
sprint_time(char* buf, char* d)
{
  sprintf(buf, "%02d:%02d:%02d",
      ((d[HR] & 0x30)>>4)*10 + (d[HR] & 0x0f),
      ((d[MIN] & 0xf0)>>4)*10 + (d[MIN] & 0x0f),
      ((d[SEC] & 0xf0)>>4)*10 + (d[SEC] & 0x0f) );
}

void
create_fname(char* d, char* fname)
{
  sprintf(fname, "%02d_%02d_%02d__%s_%02d_%02d_20%02d.txt",
      ((d[HR] & 0x30)>>4)*10 + (d[HR] & 0x0f),
      ((d[MIN] & 0xf0)>>4)*10 + (d[MIN] & 0x0f),
      ((d[SEC] & 0xf0)>>4)*10 + (d[SEC] & 0x0f),
      days[d[DAY] & 0x07],
      ((d[DATE] & 0x30)>>4)*10 + (d[DATE] & 0x0f),
      ((d[MONTH] & 0x10)>>4)*10 + (d[MONTH] & 0x0f),
      ((d[YEAR] & 0xf0)>>4)*10 + (d[YEAR] & 0x0f) );
}

void
set_rtc(char idx, char val)
{
  Wire.beginTransmission(0x68);
  Wire.write(idx);
  Wire.write(val);
  Wire.endTransmission();
}

char
dec2bcd(char val)
{
  return ( (val/10*16) + (val%10) );
}

void
handle_cmd(char* s)
{
  char d[3] = " \r";
  char *token;
  char attr[10];
  char val[14];
  String attr_s;
  char rtc_data[7];
  char show_time=0;
  int val_i;
  
  attr[0]='\0';
  val[0]='\0';
  token = strtok(s, d);
  if (token!=NULL)
  {
    strncpy(attr, token, 9);
    token = strtok(NULL, d);
    if (token!=NULL)
    {
      strncpy(val, token, 13);
    }
  }
  sscanf(val, "%d", &val_i);
  
  if (attr[0]=='\0')
  {
    printmenu();
    return;
  }
  
  attr_s=String(attr);
  if (attr_s=="hr")
  {
    set_rtc(HR, dec2bcd(val_i));
    show_time=1;
  }
  else if (attr_s=="min")
  {
    set_rtc(MIN, dec2bcd(val_i));
    show_time=1;
  }
  else if (attr_s=="sec")
  {
    set_rtc(SEC, dec2bcd(val_i));
    show_time=1;
  }
  else if (attr_s=="day")
  {
    set_rtc(DAY, dec2bcd(val_i));
    show_time=1;
  }
  else if (attr_s=="date")
  {
    set_rtc(DATE, dec2bcd(val_i));
    show_time=1;
  }
  else if (attr_s=="month")
  {
    set_rtc(MONTH, dec2bcd(val_i));
    show_time=1;
  }
  else if (attr_s=="year")
  {
    if (val_i<100)
      val_i=val_i+2000;
    set_rtc(YEAR, dec2bcd(val_i-2000));
    show_time=1;
  }
  else if (attr_s=="time")
  {
    show_time=1;
  }
  
  if (show_time)
  {
    get_rtc(rtc_data);
    serial_printf("Time: ");
    print_date(rtc_data);
    serial_printf(" ");
    print_time(rtc_data);
    serial_printf("\n\r");    
  }
}

void
menu(void)
{
  int not_finished=1;
  int idx=0;
  char mbuf[24];
  
  while(not_finished)
  {
      if (Serial.available() > 0)
      {
        mbuf[idx]=Serial.read();
        if ((mbuf[idx]=='\r') || (idx>23))
        {
          Serial.println("");
          mbuf[idx]='\0';
          handle_cmd(mbuf);
          idx=0;
          Serial.print(">"); // ready for next command
          continue;
        }
        Serial.write(mbuf[idx]);
        idx++;
      }
  }
}


void setup()
{
  pinMode(PUSH1, INPUT_PULLUP);
  pinMode(PUSH2, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(PB_2, INPUT);
  
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(RED_LED, LOW);

  Wire.setModule(1);
  Wire.begin();
  
  
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  if (PUSH2_PRESSED) // was button 2 held down at startup? If so, run the menu
  {
    digitalWrite(BLUE_LED, HIGH);
    menu();
    digitalWrite(BLUE_LED, LOW);
  }
  
  delay(3000); // wait 3 sec in case user wants to open up a serial port to view debug info
  set_rtc(0x0e, 0x00); // enable 1-sec output from RTC
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
}

void
do_logging(char* fname)
{
  int not_finished=1;
  int sensor_val;
  String datastring = "";
  char rtc_data[7];
  char tbuf[64];
  int count=0;
  int count2=0;
  float t;
  
  do
  {
    datastring="";
    dataFile = SD.open(fname, FILE_WRITE);
    
    digitalWrite(GREEN_LED, HIGH);
    while(PB2_ISLOW) // crude. Replace with an interrupt at some point
    {
      if (PUSH2_PRESSED)
        not_finished=0;
    }
    digitalWrite(GREEN_LED, LOW);
    while(PB2_ISHIGH)
    {
          if (PUSH2_PRESSED)
        not_finished=0;
    }
    
    get_rtc(rtc_data);  // *** Acquire timestamp ***
    sensor_val = analogRead(2);  // *** Acquire analog value ***
    datastring += String(sensor_val);

    if (dataFile)
    {
      sprint_time(tbuf, rtc_data);
      dataFile.print(tbuf);
      dataFile.print(", ");
      sprint_date(tbuf, rtc_data);
      dataFile.print(tbuf);
      dataFile.print(", ");
      dataFile.print(datastring);
      if (count2==0)
      {
        t=get_temp();
        sprintf(tbuf, ", %f", t);
        dataFile.print(tbuf);
      }
      dataFile.println("");
      dataFile.close();
      
      Serial.print(datastring);
      if (count2==0)
      {
        Serial.print(tbuf);
        Serial.print("degC");
      }
      count2++;
      if (count2>63)
      {
        count2=0;
      }
      count++;
      if (count>9)
      {
        Serial.println("");
        count=0;
      }
      else
      {
        Serial.print(", ");
      }
    }
    else {
      Serial.println("error opening data file");
    }
  }while(not_finished);
}

void
loop(void)
{
  char rtc_data[7];
  char fname[48];
  int n=1;
  
  Wire.setModule(1); // not sure why we have to do these commands again
  Wire.begin();
  
  while(1)
  {
    // Now we wait to be instructed to start logging
    while(PUSH1_UNPRESSED);
    digitalWrite(GREEN_LED, HIGH);
    sprintf(fname, "DAT%d.TXT", n);
    //Serial.println("Getting time...");
    //get_rtc(rtc_data);
    Serial.print("Creating filename ");
    //create_fname(rtc_data, fname);
    Serial.println(fname);
    Serial.println("Starting logging...");
    do_logging(fname);
    Serial.println("Logging stopped.");
    dataFile.close();
    n++;
    digitalWrite(GREEN_LED, LOW);
  }
}











