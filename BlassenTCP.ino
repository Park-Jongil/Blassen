#include <EEPROM.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HardwareSerial.h>
#include "FS.h"
#include "SPIFFS.h"
#include "UserDefine.h"
#include "WaterCool.h"


// ESP32-WROOM-32 기준
// GPIO01 TX0 - HardwareSerial (0), 핀번호 변경 불가
// GPIO03 RX0 - HardwareSerial (0), 핀번호 변경 불가
// GPIO10 TX1 - HardwareSerial (1), FLASH 연결에 사용, 핀번호 변경 가능
// GPIO09 RX1 - HardwareSerial (1), FLASH 연결에 사용, 핀번호 변경 가능
// GPIO17 TX2 - HardwareSerial (2), 핀번호 변경 가능
// GPIO16 RX2 - HardwareSerial (2), 핀번호 변경 가능

// HardwareSerial Serial0(0); //3개의 시리얼 중 2번 채널을 사용
// HardwareSerial Serial1(1); //3개의 시리얼 중 2번 채널을 사용
//  HardwareSerial rs485_Serial01(2);  // 장치명 선언(시리얼 번호) < 이걸로 사용해라..

// ESP32-S2-DevKitM-1 에서는 rs485_Serial01(2) 가 Serial1(1) 과 같다.
// HardwareSerial rs485_Serial01(1); ; //2개의 시리얼 중 1번 채널을 사용
// GPIO 6, 7, 8, 9, 10, 11 (the integrated SPI flash)은 플레시 메모리연결에
// 할당되어 있어서 해당핀을 사용할 수 없다

//-------------------------------------------------------------------------------
// H/W Serial
//-------------------------------------------------------------------------------
HardwareSerial rs485_Serial01(1); // 장치명 선언(시리얼 번호) - Main Controller
HardwareSerial rs485_Serial02(2); // 장치명 선언(시리얼 번호) - 대흥금속 냉난방기 모듈

//-------------------------------------------------------------------------------
// LCD 관련함수들
//-------------------------------------------------------------------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
// 통신관련된 버퍼
//-------------------------------------------------------------------------------
unsigned char       szRecvBuf[TCP_CHANNEL+1][1024];
short int           iRecvCount[TCP_CHANNEL+1] = { 0 , 0 };
unsigned char       iDeviceID = 0x01;
short int           iSerial_Menu = 0;
short int           iLastOPID = -1;
unsigned int        localPort = 5000;
//-------------------------------------------------------------------------------
// EEPROM
//-------------------------------------------------------------------------------
ProductInfo         stProductInfo;
NetworkInfo         stNetWorkInfo;
//-------------------------------------------------------------------------------
int                 iDeviceCount = 0;
DeviceInfo          stDeviceInfo[MAX_DEVICE_COUNT];
//-------------------------------------------------------------------------------

//---------------------------------------------------------------------------
uint8_t blue = 26;
//---------------------------------------------------------------------------
// Define NTP Client to get time
const char*       ntpServer = "pool.ntp.org";
const long        gmtOffset_sec = 3600 * 9;
const int         daylightOffset_sec = 0;
//---------------------------------------------------------------------------
const char* ssid     = "MZ_Guest";
const char* password = "mz@1998!!";
byte mac[] = {0x00,0xAA,0xBB,0xCC,0xDE,0x02 };
//---------------------------------------------------------------------------
int   isSFlash = true;
int   isNetwork = false;
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
WiFiServer        server(localPort);
WiFiClient        clients[TCP_CHANNEL];

//---------------------------------------------------------------------------
DeviceInfo  *DeviceInfo_Search_byID(int iChkID)
{
  for(int i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==true && stDeviceInfo[i].iDeviceID==iChkID) return(&stDeviceInfo[i]);
  }
  return(NULL);
}

//---------------------------------------------------------------------------
DeviceInfo  *DeviceInfo_Search_byMapID(int iMapID)
{
  for(int i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==true && stDeviceInfo[i].iMemoryMapID==iMapID) return(&stDeviceInfo[i]);
  }
  return(NULL);
}

//-------------------------------------------------------------------------------
void setup()
{
    int  iCount = 0,iAddr[4];
    char szBuffer[1024];
    struct tm timeinfo;
    
    
    //memset((char*)&stNetWorkInfo,0x00,sizeof(NetworkInfo));
    //memset((char*)&stProductInfo,0x00,sizeof(ProductInfo));
    //EEPROM_Set_DeviceInformation();
    
    EEPROM_Get_DeviceInformation();
    if (stProductInfo.iSlaveID < 1) stProductInfo.iSlaveID = 0x01;

    Serial.begin(115200); // UART0 -> usb 연결과 공유, 핀번호 변경 불가
                          // Serial1.begin(115200); // 통신에 할당된 기본핀이 플래시 메모리에 할당된 GPIO 10번과 9번이므로 사용불가

    if (!SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS");
      isSFlash = false;
    } else {
      isSFlash = true;
    }

    rs485_Serial01.begin(9600, SERIAL_8N1, 35, 33); // (통신속도, UART모드, RX핀번호, TX핀번호) - 핀번호 지정가능
    rs485_Serial02.begin(9600, SERIAL_8N1, 17, 16); // 핀번호는 기본으로 할당된 번호도 사용할 수 있다.
    delay(100);

// LCD 설정화면
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
      Serial.println(F("SSD1306 allocation failed"));
      for(;;);
    }
    delay(2000);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    LCD_Display_Message(0,16,"Board Initialize..");

// WIFI Setting
    Serial.println(" >>>> WiFi Connecting... Start : ");
    if (stNetWorkInfo.NetworkEnable==0x01) {
      LCD_Display_Message(0,40,"Set WIFI Setting ..");
      Serial.print(" >>>> WiFi Connecting... SSID : ");
      sprintf(szBuffer,"%s , %s",stNetWorkInfo.WIFI_SSID, stNetWorkInfo.WIFI_Password);
      Serial.println(szBuffer);
      if (strlen(stNetWorkInfo.WIFI_SSID) > 0x04) {
        WiFi.begin(stNetWorkInfo.WIFI_SSID, stNetWorkInfo.WIFI_Password);
      } else {
        WiFi.begin(ssid, password);
      }
      while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
        iCount = iCount + 1;
        if (iCount%100==0) Serial.println();
        if (iCount==200) break;            // Serial 설정을 위하여 콘솔메뉴로 갈수있도록 한다.(WIFI 재설정)
      }
      if (iCount!=200) {
        isNetwork = true;
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());    
      } else {
        Serial.println(" >>> WiFi needs to be reset.");
        Serial.println(" >>> Check the SSID and password.");
      }
// NTP 설정을 위한 부분
      if (isNetwork==true) {
        iCount = 0;
        Serial.print("NTP Time Setting = ");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        while(!getLocalTime(&timeinfo) && iCount < 5) {     // 최대 5번의 NTP 서버 시간동기화
          Serial.println("Failed to obtain time.... try again....");
          configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
          iCount = iCount + 1;
        }
        if (getLocalTime(&timeinfo)){
          Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
        }  
      }
// MQTT 설정을 위한 부분
      if (isNetwork==true) {
        sscanf(stNetWorkInfo.szIPAddress,"%d.%d.%d.%d",&iAddr[0],&iAddr[1],&iAddr[2],&iAddr[3]);
        IPAddress   SetIPAddr(iAddr[0],iAddr[1],iAddr[2],iAddr[3]);
        sscanf(stNetWorkInfo.szSubnetMask,"%d.%d.%d.%d",&iAddr[0],&iAddr[1],&iAddr[2],&iAddr[3]);
        IPAddress   SetSubnet(iAddr[0],iAddr[1],iAddr[2],iAddr[3]);
        sscanf(stNetWorkInfo.szGateWay,"%d.%d.%d.%d",&iAddr[0],&iAddr[1],&iAddr[2],&iAddr[3]);
        IPAddress   SetGateWay(iAddr[0],iAddr[1],iAddr[2],iAddr[3]);
        sscanf(stNetWorkInfo.szDNS_Addr,"%d.%d.%d.%d",&iAddr[0],&iAddr[1],&iAddr[2],&iAddr[3]);
        IPAddress   SetDNS(iAddr[0],iAddr[1],iAddr[2],iAddr[3]);
        if (strlen(stNetWorkInfo.szIPAddress) > 0) {
          WiFi.config(SetIPAddr,SetGateWay,SetSubnet,SetDNS);
        }
        Serial.print("  Real : IP address = ");
        Serial.println(WiFi.localIP());     
        Serial.print("  Real : SubnetMask = ");
        Serial.println(WiFi.subnetMask());     
        Serial.print("  Real : GateWay = ");
        Serial.println(WiFi.gatewayIP());     
        Serial.print("  Real : DNS_Addr = ");
        Serial.println(WiFi.dnsIP());     
// Server 설정
        if (stNetWorkInfo.iServerPort != 0x00) {
          server = WiFiServer(stNetWorkInfo.iServerPort); 
          server.begin();
        } else {
          server = WiFiServer(localPort); 
          server.begin();
        }        
      }       
    }

    // initialize digital pin ledPin as an output.
    pinMode(blue, OUTPUT);  

// 냉난방기 및 제습기 데이터구조체 초기화
    iDeviceCount = 0;
    for(int i=0;i<MAX_DEVICE_COUNT;i++) {
      stDeviceInfo[i].iEnabled = false;
      memset((char*)&stDeviceInfo[i],0x00,sizeof(DeviceInfo));
    }
// TCP 버퍼관련 변수들 초기화
    for(int i=0;i<(TCP_CHANNEL+1);i++) {    // TCP버퍼 및 SERIAL 버퍼
      iRecvCount[i] = 0;
      memset((char*)&szRecvBuf[i][0],0x00,BUFFER_SIZE);
    }      
// rs485_Serial02 를 통한 Slave 장비검색(초기에 한번만 수행 , 장비가 새로 추가되었을 경우에 한시간에 한번 재검색으로 동기화)
    SlaveDevice_ScanbyTime();  
    
    Serial_Display_MainDevice_Information();
}

//-------------------------------------------------------------------------------
// EEPROM 에 저장되어 있는 설정값을 획득한다. 
//-------------------------------------------------------------------------------
void  EEPROM_Get_DeviceInformation()
{
  int   iPosition=0x00;

  EEPROM.begin(sizeof(NetworkInfo)+sizeof(ProductInfo));
  EEPROM.get(iPosition,stNetWorkInfo);     // Network 정보를 읽는다.
  iPosition = sizeof(NetworkInfo);
  EEPROM.get(iPosition,stProductInfo);     // 장비의 설정정보를 읽는다.
  iPosition = sizeof(NetworkInfo)+sizeof(ProductInfo);
}

//-------------------------------------------------------------------------------
// EEPROM 에 저장되어 있는 설정값을 획득한다. 
//-------------------------------------------------------------------------------
void  EEPROM_Set_DeviceInformation()
{
  int   iPosition=0x00;

  EEPROM.begin(sizeof(NetworkInfo)+sizeof(ProductInfo));
  EEPROM.put(iPosition,stNetWorkInfo);     // Network 정보를 읽는다.
  iPosition = sizeof(NetworkInfo);
  EEPROM.put(iPosition,stProductInfo);     // 장비의 설정정보를 읽는다.
  iPosition = sizeof(NetworkInfo)+sizeof(ProductInfo);
  EEPROM.commit();
}


//---------------------------------------------------------------------------
char  szMqttMessage[1024];
//---------------------------------------------------------------------------
void LCD_Display_MQTT_Message(int Cx,int Cy,char *szMessage)
{
  char szBuffer[1024],szTemp[16];
  struct tm timeinfo;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  if (getLocalTime(&timeinfo)) {
    sprintf(szBuffer,"%d-%d-%d %d:%d:%d",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);  
    display.println(szBuffer);
  }
  display.setCursor(0, 12);
  sprintf(szBuffer,"MQTT : %s",szMessage);
  strcpy(szMqttMessage,szBuffer);
  display.println(szBuffer);
  display.display();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void LCD_Display_Recv_Message(int Cx,int Cy,int iCmdType,int MsgSize,unsigned char *szMessage)
{
  char szBuffer[1024],szTemp[16];
  struct tm timeinfo;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  if (getLocalTime(&timeinfo)) {
    sprintf(szBuffer,"%d-%d-%d %d:%d:%d",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);  
    display.println(szBuffer);
  }
  display.setCursor(0, 12);
  display.println(szMqttMessage);
  display.setCursor(0, 28);
  sprintf(szBuffer,"%s [ %d ]",iCmdType==1?"KSX3267":"WaterCooler", MsgSize);
  display.println(szBuffer);
  display.setCursor(0, 40);
  memset(szBuffer,0x00,sizeof(szBuffer));
  for(int i=0;i<MsgSize && i<32;i++) {
    sprintf(szTemp,"%02X ",szMessage[i]);
    strcat(szBuffer,szTemp);
  }
  display.println(szBuffer);
  display.display();
  memset(szMqttMessage,0x00,sizeof(szMqttMessage));
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void LCD_Display_Message(int Cx,int Cy,char *szMessage)
{
  char szBuffer[1024];
  struct tm timeinfo;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  if (getLocalTime(&timeinfo)) {
    sprintf(szBuffer,"%d-%d-%d %d:%d:%d",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);  
    display.println(szBuffer);
  }
  display.setCursor(Cx, Cy);
  sprintf(szBuffer,"%-20s", szMessage);
  display.println(szBuffer);
  display.display();
}


//-------------------------------------------------------------------------------
void Serial_Display_DebugMessage(int iMode,char *szChannel,unsigned char *szCommand, int iCount)
{
  char  szBuffer[128];

  sprintf(szBuffer," >> %s [%s] [%2d] : ",(iMode==0x01?"Send":"Recv"),szChannel,iCount);
  Serial.print(szBuffer);
  for(int i=0;i<iCount && i<32;i++) {
    sprintf(szBuffer,"%02X ",szCommand[i]);
    Serial.print(szBuffer);
  }
  Serial.println();
}



//---------------------------------------------------------------------------
unsigned short int ModRTU_CRC(unsigned char *buf, int len)
{
  unsigned short int crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (unsigned short int)buf[pos];          // XOR byte into least sig. byte of crc
    for (int i = 8; i != 0; i--) {        // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      } else {                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
      }
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
unsigned short int Convert_Endian(unsigned short int iChkData)
{
  unsigned short int   iChkRet;

  iChkRet = (iChkData % 0x100) * 0x100 + (iChkData / 0x100);
  return (iChkRet);
}

//-------------------------------------------------------------------------------
// MainSerial(USB) 을 통하여 메뉴모드로 동작중
//-------------------------------------------------------------------------------
int   iSerialCmd = 0;
char  szSerialCmd[64];
//-------------------------------------------------------------------------------
int  Serial_Menu_Get_String()
{
  unsigned char   nData;

  if (iSerialCmd==0x00) memset(szSerialCmd,0x00,sizeof(szSerialCmd));
  while (Serial.available() > 0) {
    nData = Serial.read();
    szSerialCmd[iSerialCmd] = nData;
    iSerialCmd = iSerialCmd + 1;
  }
  if (nData==0x0A) return(0x01); 
  return(0x00);  
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
void  Serial_Menu_Display(int iMenu)
{
  char    szBuffer[64];
  struct tm timeinfo;  
  time_t  now;
    
  if (iSerial_Menu==0x00) {       // Main Menu
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(szBuffer,"CurTime = %04d-%02d-%02d %02d:%02d:%02d",1900+timeinfo.tm_year,timeinfo.tm_mon+1,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    Serial.println(szBuffer);
    Serial.println("0.WaterCooler Management Program");
    Serial.println("  [0] Check Entire Device");
    Serial.println("  [1] Device Network Setting");
    Serial.println("  [2] Slave Device Informations");
    Serial.println("  [9] System Restart..");
  } else if (iSerial_Menu==0x01) {
    Serial.println("1.Device Network Setting");
    Serial.println("  [0] return  ");
    Serial.println("  [1] Current Network Infomations  ");
    Serial.println("  [2] IP Address  ");
    Serial.println("  [3] Subnet  ");
    Serial.println("  [4] GateWay  ");
    Serial.println("  [5] DNS  ");
    Serial.println("  [6] Server Port  ");
    Serial.println("  [7] WIFI SSID  ");
    Serial.println("  [8] WIFI Password  ");
    Serial.println("  [9] Network Enable");
    Serial.println(" [10] EEPROM Save");
  } else if (iSerial_Menu==0x02) {
    Serial.println("2.Slave Device Informations");
    Serial.println("  [0] return");
    Serial.println("  [1] Display Device ID/Type");
  }
}

//-------------------------------------------------------------------------------
// MainSerial(USB) 을 통하여 메뉴모드로 동작중
//-------------------------------------------------------------------------------
void  Serial_Menu_Command()
{
  int   iRet=0,iChannel,iCount;
  char  szBuffer[128];

  iRet = Serial_Menu_Get_String();
  if (iRet==0x00) return;  
  iSerialCmd = 0;
  Serial.print(" ==> Command = ");       
  for(int i=0;i<strlen(szSerialCmd);i++) {
    sprintf(szBuffer,"%02X ",szSerialCmd[i]);
    Serial.print(szBuffer);       
  }      
  Serial.println();       
  for(int i=0;i<strlen(szSerialCmd);i++) {
    if (szSerialCmd[i]==0x0D) szSerialCmd[i]=0x00;
  }
 
// 엔터까지 기다려서 명령을 확인하는 것으로 한다.   
  if (iSerial_Menu==0x00) {       // Main Menu
    if (strcmp(szSerialCmd,"0")==0x00) {
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"1")==0x00) {
      iSerial_Menu = 0x01;
      Serial_Display_MainDevice_Information();
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"2")==0x00) {
      iSerial_Menu = 0x02;
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"9")==0x00) {
      Serial.print("System Reset.....");  
      ESP.restart();    
    }
  } else if (iSerial_Menu==0x01) {
    if (strcmp(szSerialCmd,"0")==0x00) {
      iSerial_Menu = 0x00;   
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"1")==0x00) {
      sprintf(szBuffer,"  Network Enable = %d",stNetWorkInfo.NetworkEnable);
      Serial.println(szBuffer);       
      sprintf(szBuffer,"  Network Info = %s [%s],[%s],[%s]",stNetWorkInfo.szIPAddress,stNetWorkInfo.szSubnetMask,stNetWorkInfo.szGateWay,stNetWorkInfo.szDNS_Addr);
      Serial.println(szBuffer);       
      sprintf(szBuffer,"  Server Port = %d",stNetWorkInfo.iServerPort);
      Serial.println(szBuffer);       
      sprintf(szBuffer,"  WIFI SSID = %s , %s",stNetWorkInfo.WIFI_SSID,stNetWorkInfo.WIFI_Password);
      Serial.println(szBuffer);       
      Serial_Menu_Display(iSerial_Menu);       
    } else if (strcmp(szSerialCmd,"2")==0x00) {
      Serial.print("  IP Address = ");  
      iSerial_Menu = 0x11;     
    } else if (strcmp(szSerialCmd,"3")==0x00) {
      Serial.print("  Subnet Mask = ");  
      iSerial_Menu = 0x12;     
    } else if (strcmp(szSerialCmd,"4")==0x00) {
      Serial.print("  Gateway IP = ");  
      iSerial_Menu = 0x13;     
    } else if (strcmp(szSerialCmd,"5")==0x00) {
      Serial.print("  DNS IP = ");  
      iSerial_Menu = 0x14;     
    } else if (strcmp(szSerialCmd,"6")==0x00) {
      Serial.print("  Server Port = ");  
      iSerial_Menu = 0x15;     
    } else if (strcmp(szSerialCmd,"7")==0x00) {
      Serial.print("  WIFI SSID = ");  
      iSerial_Menu = 0x16;     
    } else if (strcmp(szSerialCmd,"8")==0x00) {
      Serial.print("  WIFI Password = ");  
      iSerial_Menu = 0x17;     
    } else if (strcmp(szSerialCmd,"9")==0x00) {
      Serial.print("  Network Enable = ");  
      iSerial_Menu = 0x19;     
    } else if (strcmp(szSerialCmd,"10")==0x00) {
      EEPROM_Set_DeviceInformation();
      iSerial_Menu = 0x01;  
      Serial_Menu_Display(iSerial_Menu);    
    }
  } else if (iSerial_Menu==0x02) {      // 3.Slave Device Informations
    if (strcmp(szSerialCmd,"0")==0x00) {
      iSerial_Menu = 0x00;  
      Serial_Menu_Display(iSerial_Menu); 
      return;
    } else if (strcmp(szSerialCmd,"1")==0x00) {     // Display Device Info
      Serial.println("Slave Device Informations ........"); 
      iCount = 0;
      for(int i=0;i<MAX_DEVICE_COUNT;i++) {
        if (stDeviceInfo[i].iEnabled==true) {
          iCount = iCount + 1;
          sprintf(szBuffer,"ID=%02d [%s] ",stDeviceInfo[i].iDeviceID,stDeviceInfo[i].iDeviceType==0x01?"Cooler":"Dehumidifier");
          Serial.println(szBuffer); 
        }
      }
      sprintf(szBuffer,"  ......... Total Device Count = %d",iCount);
      Serial.println(szBuffer); 
      Serial_Menu_Display(iSerial_Menu); 
    }
  } else if (iSerial_Menu==0x11) {
    strcpy(stNetWorkInfo.szIPAddress,szSerialCmd);  
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  } else if (iSerial_Menu==0x12) {
    strcpy(stNetWorkInfo.szSubnetMask,szSerialCmd);  
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  } else if (iSerial_Menu==0x13) {
    strcpy(stNetWorkInfo.szGateWay,szSerialCmd);  
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  } else if (iSerial_Menu==0x14) {
    strcpy(stNetWorkInfo.szDNS_Addr,szSerialCmd);  
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  } else if (iSerial_Menu==0x15) {
    stNetWorkInfo.iServerPort = atoi(szSerialCmd);  
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  } else if (iSerial_Menu==0x16) {
    strcpy(stNetWorkInfo.WIFI_SSID,szSerialCmd);  
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  } else if (iSerial_Menu==0x17) {
    strcpy(stNetWorkInfo.WIFI_Password,szSerialCmd);  
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  } else if (iSerial_Menu==0x19) {
    stNetWorkInfo.NetworkEnable = atoi(szSerialCmd);   
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  }
}

//-------------------------------------------------------------------------------
// MainSerial(USB) - ESP32 Device Information
//-------------------------------------------------------------------------------
void Serial_Display_MainDevice_Information()
{
  int     i;
  char    szBuffer[1024];

  Serial.println("...Display Main Device Informations...");
  sprintf(szBuffer,"DeviceID(485) = %d",stProductInfo.iSlaveID);
  Serial.println(szBuffer);
  Serial.println("...");
}

//-------------------------------------------------------------------------------
// MainSerial(USB) - WaterCooler Device Information
//-------------------------------------------------------------------------------
void Serial_Display_WaterCooler_Information()
{
  int     i;
  char    szBuffer[1024];

  Serial.println("...Display WaterCooler Device Informations...");
  Serial.println("SEQ\tDeviceID\tKSX_ID\tON/OFF\tSystemMode\tTEMP");
  for(i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==true) {
      sprintf(szBuffer,"%02d\t%02d\t\t%03d\t%02d\t%03d\t\t%03d",i,stDeviceInfo[i].iDeviceID,stDeviceInfo[i].iMemoryMapID,stDeviceInfo[i].iOperation,stDeviceInfo[i].iSystemMode,stDeviceInfo[i].iCurTemperature);
      Serial.println(szBuffer);
    }
  }
  Serial.println("...");
}


//-------------------------------------------------------------------------------
// WaterCooler 에게 보내는 명령(보드기준 오른쪽)
//-------------------------------------------------------------------------------
void  SendMessage_ToDevice(unsigned char *szSendMsg, int iCount)
{
//  digitalWrite(RS485_TX_ENABLE, HIGH);
  rs485_Serial02.write(szSendMsg, iCount);
  rs485_Serial02.flush();
//  digitalWrite(RS485_TX_ENABLE, LOW);
}

//-------------------------------------------------------------------------------
// TCP로 전달하기 위한 소켓구조체
//-------------------------------------------------------------------------------
void  SendMessage_ToServer(unsigned char *szSendMsg, int iCount)
{
  for (byte i = 0; i < TCP_CHANNEL; i++) {
    if (clients[i] && !clients[i].connected()) {
      clients[i].write_P((const char*)szSendMsg,(size_t)iCount);
    }
  }
}

//---------------------------------------------------------------------------
// 통계에 관련된 온도/습도를 시간단위로 저장한다.
//---------------------------------------------------------------------------
void  FileStore_Statistics_Data()
{
  char    szBuffer[1024],szFileName[128];
  File    fp;
  struct  tm timeinfo;

  if (isSFlash==true) {
    for(int i=0;i<MAX_DEVICE_COUNT;i++) {
      if (stDeviceInfo[i].iEnabled==0x01) {
        memset(szBuffer,0x00,sizeof(szBuffer));
        memset(szFileName,0x00,sizeof(szFileName));
        if (getLocalTime(&timeinfo)){
          sprintf(szFileName,"T%02d_%04d%02d",stDeviceInfo[i].iDeviceID,1900+timeinfo.tm_year,timeinfo.tm_mon);  
          if (SPIFFS.exists(szFileName)==0) {   // 파일이 존재하지 않으면 생성하며 31*24 의 버퍼를 미리 생성한다.
            fp = SPIFFS.open(szFileName, "w");     
            if (!fp) {
              fp.seek(0, SeekSet);
              fp.write((const uint8_t*)szBuffer,31*24);
              fp.close();
            }
          }
          fp = SPIFFS.open(szFileName, "w");     
          if (!fp) {
            fp.seek(24*timeinfo.tm_hour, SeekSet);
            fp.write((const uint8_t*)stDeviceInfo[i].iStatTemperature,24);
            fp.close();
          }
          if (stDeviceInfo[i].iDeviceType==0x02) {
            sprintf(szFileName,"H%02d_%04d%02d",stDeviceInfo[i].iDeviceID,1900+timeinfo.tm_year,timeinfo.tm_mon);   
            if (SPIFFS.exists(szFileName)==0) {   // 파일이 존재하지 않으면 생성하며 31*24 의 버퍼를 미리 생성한다.
              fp = SPIFFS.open(szFileName, "w");     
              if (!fp) {
                fp.seek(0, SeekSet);
                fp.write((const uint8_t*)szBuffer,31*24);
                fp.close();
              }
            }                
            fp = SPIFFS.open(szFileName, "w");     
            if (!fp) {
              fp.seek(24*timeinfo.tm_hour, SeekSet);
              fp.write((const uint8_t*)stDeviceInfo[i].iStatHumidity,24);
              fp.close();
            }
          }   
        }
        stDeviceInfo[i].iSumTemperature = 0;
        stDeviceInfo[i].iSumHumidity = 0;
        stDeviceInfo[i].iGetCount = 0;
      }
    }  
  }
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void loop()
{
  char  szBuffer[512],szDateTime[128];
  unsigned char   nData;
  static int      iTimeCount = 0;
  struct tm       timeinfo;
  static unsigned long timepoint = millis();

  if ((millis()-timepoint)>(950U)) {                  //time interval: 0.8s , 1초마다하면 특정초를 넘어갈수있다.  0.99 --> 2.01
    timepoint = millis();
    if (getLocalTime(&timeinfo)) {
      sprintf(szDateTime,"%d-%02d-%02d %02d:%02d:%02d",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);  
      if (timeinfo.tm_hour==23 && timeinfo.tm_min==59 && timeinfo.tm_sec==10) {   // 하루에 한번 시간동기화
        sprintf(szBuffer,"    [%s] >> Time Sync",szDateTime);
        Serial.println(szBuffer);                           // 수신된 메세지가 정상적으로 파싱되었다는것을 출력      
        SlaveDevice_SetSystemTime();    
      }
      if (timeinfo.tm_min%2==0 && timeinfo.tm_sec==0) {    // 장비스캔 10분에 한번씩
        LCD_Display_Message(0,12,"Get Status Polling..");
        sprintf(szBuffer,"    [%s] >> Get Status Polling",szDateTime);
        Serial.println(szBuffer);                           // 
        if (iDeviceCount==0x00) SlaveDevice_ScanbyTime();   // 장비스캔전에 장비가 없으면 스캔기능 동작.
         else SlaveDevice_GetStatusbyTime();
      }
      if (timeinfo.tm_min==1 && timeinfo.tm_sec==10) {      // 추가된 장비가 있는지 확인하는 장비스캔기능 
        LCD_Display_Message(0,12,"New Device Scan..");
        sprintf(szBuffer,"    [%s] >> New Device Scan",szDateTime);
        Serial.println(szBuffer);                           // 
        SlaveDevice_ScanbyTime(); 
      }
      if (timeinfo.tm_min==59 && timeinfo.tm_sec==20) {      // 파일저장루틴 활성화
        LCD_Display_Message(0,12,"Temp/Humidity FileSave");
        sprintf(szBuffer,"    [%s] >> File Store",szDateTime);
        Serial.println(szBuffer);                           // 
        FileStore_Statistics_Data();
      }
    }
  }
  
// Ethernet 을 통하여 새로운 접속이 있었을 경우에 SocketID를 신규로 할당하여 처리.  
  WiFiClient newClient = server.accept();
  if (newClient) {
    for (byte i = 0; i < TCP_CHANNEL; i++) {
      if (!clients[i]) {
        newClient.print("Hello, client number: ");
        newClient.println(i);
        // Once we "accept", the client is no longer tracked by EthernetServer
        // so we must store it into our list of clients
        clients[i] = newClient;
        sprintf(szBuffer,"TCP Client Connect #%d",i);
        Serial.println(szBuffer);        
        break;
      }
    }
  }
// stop any clients which disconnect
  for (byte i = 0; i < TCP_CHANNEL; i++) {
    if (clients[i] && !clients[i].connected()) {
      clients[i].stop();
      iRecvCount[i] = 0;
      sprintf(szBuffer,"TCP Client Disconnect #%d",i);
      Serial.println(szBuffer);        
    }
  }
// check for incoming data from all clients
  for (byte i = 0; i < TCP_CHANNEL; i++) {
    while (clients[i] && clients[i].available() > 0) {
      // read incoming data from the client
      szRecvBuf[i][iRecvCount[i]] = clients[i].read();
      iRecvCount[i]++;
    }
  }

// Serial#2 에 데이터가 수신되었는지를 확인하여 버퍼에 저장 (대흥금속 냉난방기 콘트롤러)
  while (rs485_Serial02.available() > 0) {
    nData = rs485_Serial02.read();
    szRecvBuf[2][iRecvCount[2]] = nData;
    iRecvCount[2]++;
  }


// Serial#1 의 버퍼에 내용이 있는지 확인하여 대흥금속 프로토콜 검사
//  if (iRecvCount[0] > 0) KSX3267_Receive_Message();  
// Serial#2 의 버퍼에 내용이 있는지 확인하여 대흥금속 프로토콜 검사
  if (iRecvCount[2] > 0) WaterCooller_Receive_Message(0x02);  

// TCP Buffer 에 내용이 있을 경우에 처리를 한다.
  for (byte i = 0; i < TCP_CHANNEL; i++) {
    if (iRecvCount[i] > 0) Protocol_TCP_Receive_Message(i);  
  }

// Serial 통신을 이용하여 TCP 데이터 확인
  while (Serial.available() > 0) {
    Serial_Menu_Command();
  }
   
}


