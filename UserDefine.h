#ifndef User_DataStruct_Define
#define User_DataStruct_Define
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#define   ERROR_NO_FUNCTION      0x01
#define   ERROR_DATA_ADDR        0x02
#define   ERROR_DATA_VALUE       0x03
#define   ERROR_DEV_FAIL         0x04
//---------------------------------------------------------------------------
#define   TCP_CHANNEL           2
#define   MAX_DEVICE_COUNT      20
#define   TEMPERATURE_MARGIN    40
#define   BUFFER_SIZE           1024
//---------------------------------------------------------------------------
typedef struct  _ProductInfo {
  short int   iSlaveID;               // RS485용 구분을 위한 장치 ID
  char        szCompanyName[32];
  char        szProductName[32];
  char        szProductCode[32];
  char        szSerialNumber[32];     // 제품시리얼번호
  char        szRouterNumber[16];     // 전화번호 이며, MQTT Topic 로 사용될 예정 (예: /Blassen/01097974226/ )
  char        szMQTT_Server[64];      // XXX.XXX.XXX.XXX or URL
  short int   iMQTT_Port;             // MQTT Broker Port [Default:1883] 
} ProductInfo;

typedef struct  _NetworkInfo {
  char        NetworkEnable;
  char        szIPAddress[32];
  char        szSubnetMask[32];
  char        szGateWay[32];
  char        szDNS_Addr[32];
  short int   iServerPort;
  char        WIFI_SSID[32];              // Wifi SSID
  char        WIFI_Password[32];          // Wifi Password
} NetworkInfo;

//---------------------------------------------------------------------------
// Device Info Structure
//---------------------------------------------------------------------------
typedef struct  _DeviceInfo {
  unsigned char   iEnabled;
  unsigned int    iMemoryMapID;
  unsigned char   iDeviceID;        // 1~10:냉낭방기
  unsigned char   iDeviceType;      // 0x01:냉난방기 , 0x02:제습기
  unsigned char   iSystemMode;
  unsigned char   iPower_Mode;      // 0x00:OFF , 0x01:ON , 0x02:SystemMode
  unsigned char   iOperation;  
  unsigned char   iCurTemperature;
  unsigned char   iSetTemperature;
  short int       iErrorStatus;
  short int       iValveStatus;
  short int       iAutoTemp;
  short int       iAutoUsed;
  short int       iCurHumidity;
  short int       iSetHumidity;
  short int       iSumTemperature;
  short int       iSumHumidity;
  char            iStatTemperature[24];      // 24 * 31
  char            iStatHumidity[24];
  int             iRetryCount;
  int             iGetCount;
} __attribute__ ((packed)) DeviceInfo;

//-------------------------------------------------------------------------------
// 대흥금속 냉난방기 프로토콜 적용
//-------------------------------------------------------------------------------
typedef struct  _RequestPacket {
  unsigned char   STX;            // STX : 0x02
  unsigned char   SID;            // SlaveID : 0x01 ~ 0xF0
  unsigned char   CMD;            // Command Code
  short int       LEN;            // Data Length
  unsigned char   DATA[1024];      // DataBlock , ETX(0x03)
} __attribute__ ((packed)) RequestPacket; 

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
typedef struct  _RequestDehumidity {
  unsigned char   iCurTemp;
  unsigned char   iCurHumidity;
  unsigned char   iPower_Mode;
  unsigned char   iSystemMode;
  unsigned char   iSetTemp;
  unsigned char   iSetHumidity;
} __attribute__ ((packed)) RequestDehumidity;

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
typedef struct  _RequestTempSetting {
  unsigned char   SetTemp[24];    
  unsigned char   UseTemp[24];            
  unsigned char   AutoTemp;            
  short int       AutoUse;            
} __attribute__ ((packed)) RequestTempSetting; 

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
typedef struct  _RequestStatistics {
  unsigned char   iDataType;
  short int       Year;
  unsigned char   Month;
} __attribute__ ((packed)) RequestStatistics;

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
typedef struct  _RequestDateTime {
  unsigned short int       Year;
  unsigned char   Month;
  unsigned char   Day;
  unsigned char   Hour;
  unsigned char   Minute;
  unsigned char   Second;
} __attribute__ ((packed)) RequestDateTime;  

//---------------------------------------------------------------------------
// Public Functions
//---------------------------------------------------------------------------
unsigned short int ModRTU_CRC(unsigned char *buf, int len);
unsigned short int Convert_Endian(unsigned short int iChkData);
unsigned int Convert_Endian_Integer(unsigned int iChkData);
void  SendMessage_ToDevice(unsigned char *szSendMsg, int iCount);
void  SendMessage_ToServer(unsigned char *szSendMsg, int iCount);
DeviceInfo  *DeviceInfo_Search_byID(int iChkID);
DeviceInfo  *DeviceInfo_Search_byMapID(int iMapID);

void LCD_Display_Message(int Cx,int Cy,char *szMessage);
void LCD_Display_Recv_Message(int Cx,int Cy,int iCmdType,int MsgSize,unsigned char *szMessage);
void Serial_Display_DebugMessage(int iMode,char *szChannel,unsigned char *szCommand, int iCount);
void Protocol_TCP_Receive_Message(int iChannel);

//---------------------------------------------------------------------------
#endif
