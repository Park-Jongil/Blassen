//#include <ESP32Ticker.h>
#include <stdio.h>
#include <Ticker.h>
#include "Arduino.h"
#include "UserDefine.h"
#include "WaterCool.h"

//-------------------------------------------------------------------------------
// Main Module Variable
//-------------------------------------------------------------------------------
extern int                 iDeviceCount;
extern DeviceInfo          stDeviceInfo[MAX_DEVICE_COUNT];
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
// 통신관련된 버퍼
//-------------------------------------------------------------------------------
extern unsigned char       szRecvBuf[TCP_CHANNEL+1][1024];
extern short int           iRecvCount[TCP_CHANNEL+1];
extern unsigned char       iDeviceID;

//-------------------------------------------------------------------------------
// 시간지연을 위한 타이머
//---------------------------------------------------------------------------
Ticker DelaySend;

//---------------------------------------------------------------------------
// 장비가 초기부팅시에 전체 SlaveDevice에 대해서 ID 조회를 요청한다.  
//---------------------------------------------------------------------------
void SlaveDevice_ScanbyTime()
{
    RequestPacket       stResponse;
    unsigned short int  iChkCRC;

    iRecvCount[1] = 0;
    for(int i=0;i<MAX_DEVICE_COUNT;i++) {
      stDeviceInfo[i].iEnabled = false;
      SlaveDevice_GetDeviceID(0x01 + i , 0x31);
      delay(50);
    }
}

//---------------------------------------------------------------------------
// Get DeviceID( Code:0x31,0x31 , DataLen : 0 )  
//---------------------------------------------------------------------------
void SlaveDevice_GetDeviceID(int iID,int iCmd)
{
    RequestPacket       stResponse;
    unsigned short int  iChkCRC;

    iRecvCount[1] = 0;
    stResponse.STX = 0x02;
    stResponse.SID = iID;
    stResponse.CMD = iCmd;  //0x31;
    stResponse.LEN = 0x00;
    iChkCRC = ModRTU_CRC((unsigned char*)&stResponse,stResponse.LEN+5);
    stResponse.DATA[stResponse.LEN+0] = iChkCRC >> 8;;
    stResponse.DATA[stResponse.LEN+1] = iChkCRC % 0x100;;
    SendMessage_ToDevice((unsigned char*)&stResponse,stResponse.LEN+7);
}

//---------------------------------------------------------------------------
// Set DeviceID( Code:0x32,0x12 , DataLen : 0 )  
//---------------------------------------------------------------------------
void SlaveDevice_SetDeviceID(int iID,int iCmd,int iSID)
{
    RequestPacket       stResponse;
    unsigned short int  iChkCRC;

    iRecvCount[1] = 0;
    stResponse.STX = 0x02;
    stResponse.SID = iID;
    stResponse.CMD = iCmd;  //0x12;
    stResponse.LEN = 0x01;
    stResponse.DATA[0] = iSID;
    iChkCRC = ModRTU_CRC((unsigned char*)&stResponse,stResponse.LEN+5);
    stResponse.DATA[stResponse.LEN+0] = iChkCRC >> 8;;
    stResponse.DATA[stResponse.LEN+1] = iChkCRC % 0x100;;
    SendMessage_ToDevice((unsigned char*)&stResponse,stResponse.LEN+7);
}

//---------------------------------------------------------------------------
// GetStatus( Code:0x33,0x38 , DataLen : 0 )  
//---------------------------------------------------------------------------
void SlaveDevice_GetStatusbyTime()
{
  unsigned short int  iChkCRC;
  RequestPacket       stResponse;

  iRecvCount[1] = 0;      // 수신버퍼 초기화
  for(int i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==0x01) {
      stDeviceInfo[i].iCurTemperature  = 0;      // 
      stDeviceInfo[i].iCurHumidity     = 0;      // 
      stResponse.STX = 0x02;
      stResponse.SID = stDeviceInfo[i].iDeviceID;
      stResponse.CMD = stDeviceInfo[i].iDeviceType==0x01 ? 0x33:0x38;
      stResponse.LEN = 0x00;
      iChkCRC = ModRTU_CRC((unsigned char*)&stResponse,stResponse.LEN+5);
      stResponse.DATA[stResponse.LEN+0] = iChkCRC / 0x100;;
      stResponse.DATA[stResponse.LEN+1] = iChkCRC % 0x100;;
      SendMessage_ToDevice((unsigned char*)&stResponse,stResponse.LEN+7);
    }
  }
}


//---------------------------------------------------------------------------
// Set SystemMode Response( Code:0x14 , DataLen : 3 ) :
//---------------------------------------------------------------------------
void SlaveDevice_SetSystemMode(int iDeviceID,int iCmd,int iCmdOP,int iSystemMode,int iSetTemp)
{
    RequestPacket       stResponse;
    unsigned short int  iChkCRC;

    iRecvCount[1] = 0;
    stResponse.STX = 0x02;
    stResponse.SID = iDeviceID;
    stResponse.CMD = iCmd;  //0x14;
    stResponse.LEN = 0x03;
    stResponse.DATA[0] = iCmdOP;
    stResponse.DATA[1] = iSystemMode;
    stResponse.DATA[2] = iSetTemp;
    iChkCRC = ModRTU_CRC((unsigned char*)&stResponse,stResponse.LEN+5);
    stResponse.DATA[stResponse.LEN+0] = iChkCRC >> 8;;
    stResponse.DATA[stResponse.LEN+1] = iChkCRC % 0x100;;
    SendMessage_ToDevice((unsigned char*)&stResponse,stResponse.LEN+7);
}

//---------------------------------------------------------------------------
// Set SystemSetting ( Code:0x36 , DataLen : 24+24+1+1 ) : 시간/온도 설정변경
//---------------------------------------------------------------------------
void SlaveDevice_SetSystemSetting(int iDeviceID,int iCmd,RequestTempSetting *pChkPnt)
{
  RequestPacket     stSendMsg;
  unsigned short int  iChkCRC;

  iRecvCount[1] = 0;          // 수신버퍼 초기화
  stSendMsg.STX = 0x02;
  stSendMsg.SID = iDeviceID;
  stSendMsg.CMD = iCmd;       // 0x16 , 0x36
  stSendMsg.LEN = sizeof(RequestTempSetting);
  iChkCRC = ModRTU_CRC((unsigned char*)&stSendMsg,stSendMsg.LEN+5);
  stSendMsg.DATA[stSendMsg.LEN+0] = iChkCRC >> 8;;
  stSendMsg.DATA[stSendMsg.LEN+1] = iChkCRC % 0x100;;
  SendMessage_ToDevice((unsigned char*)&stSendMsg,stSendMsg.LEN+7);
}


//---------------------------------------------------------------------------
// Set SystemTime ( Code:0x37 , DataLen : YY,MM,DD,HH,MM,SS ) : 전체장비 리스트를 획득
//---------------------------------------------------------------------------
void SlaveDevice_SetSystemTime()
{
  RequestPacket       stSendMsg;
  RequestDateTime     stDateTime;
  unsigned short int  iChkCRC;
  struct tm timeinfo;

  for(int i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==0x01) {
      if (getLocalTime(&timeinfo)){
        stDateTime.Year  = timeinfo.tm_year + 1900;
        stDateTime.Month = timeinfo.tm_mon;
        stDateTime.Day   = timeinfo.tm_mday;
        stDateTime.Hour  = timeinfo.tm_hour;
        stDateTime.Minute = timeinfo.tm_min;
        stDateTime.Second = timeinfo.tm_sec;
        stSendMsg.STX = 0x02;
        stSendMsg.SID = stDeviceInfo[i].iDeviceID;
        stSendMsg.CMD = 0x37;
        stSendMsg.LEN = sizeof(RequestDateTime);
        memcpy((char*)&stSendMsg.DATA[0],(char*)&stDateTime,sizeof(RequestDateTime));
        iChkCRC = ModRTU_CRC((unsigned char*)&stSendMsg,stSendMsg.LEN+5);
        stSendMsg.DATA[stSendMsg.LEN+0] = iChkCRC / 0x100;
        stSendMsg.DATA[stSendMsg.LEN+1] = iChkCRC % 0x100;
        SendMessage_ToDevice((unsigned char*)&stSendMsg,stSendMsg.LEN+7);
      }
    }
  }
}

//---------------------------------------------------------------------------
// Set SystemMode(제습기) ( Code:0x19 , DataLen : 4 ) :
//---------------------------------------------------------------------------
void SlaveDevice_SetSystemMode_Humidify(int iDeviceID,int iCmd,int iPowerMode,int iSystemMode,int iSetTemp,int iSetHudm)
{
    RequestPacket       stResponse;
    unsigned short int  iChkCRC;

    iRecvCount[1] = 0;
    stResponse.STX = 0x02;
    stResponse.SID = iDeviceID;
    stResponse.CMD = iCmd;  //0x14;
    stResponse.LEN = 0x04;
    stResponse.DATA[0] = iPowerMode;
    stResponse.DATA[1] = iSystemMode;
    stResponse.DATA[2] = iSetTemp;
    stResponse.DATA[3] = iSetHudm;
    iChkCRC = ModRTU_CRC((unsigned char*)&stResponse,stResponse.LEN+5);
    stResponse.DATA[stResponse.LEN+0] = iChkCRC >> 8;;
    stResponse.DATA[stResponse.LEN+1] = iChkCRC % 0x100;;
    SendMessage_ToDevice((unsigned char*)&stResponse,stResponse.LEN+7);
}

//---------------------------------------------------------------------------
// 장비ID 조회 요청메세지( Code:0xB1 , DataLen : 0 ) : 전체장비 리스트를 획득
//---------------------------------------------------------------------------
void Protocol_Parser_GetID(RequestPacket *pChkPnt)
{
  int         iDeviceID,iMapID;
  DeviceInfo  *pFindPnt;

  iDeviceID = pChkPnt->DATA[0];
  pFindPnt = DeviceInfo_Search_byID(iDeviceID);
  if (pFindPnt==NULL && iDeviceCount < MAX_DEVICE_COUNT) {
    stDeviceInfo[iDeviceCount].iEnabled = true;
    stDeviceInfo[iDeviceCount].iDeviceID = pChkPnt->DATA[0];
    stDeviceInfo[iDeviceCount].iDeviceType = pChkPnt->DATA[1];
    iDeviceCount = iDeviceCount + 1;
  } else {
    pFindPnt->iEnabled = true;
  }
}

//---------------------------------------------------------------------------
// Get SystemID Response( Code:0xB2,0x92 , DataLen : 1 , [ ID ] ) : 
//---------------------------------------------------------------------------
void Protocol_Parser_SetID(RequestPacket *pChkPnt)
{
  int   iDeviceID;
  DeviceInfo  *pFindPnt;

  iDeviceID = pChkPnt->DATA[0];
  pFindPnt = DeviceInfo_Search_byID(iDeviceID);
  if (pFindPnt!=NULL) {
    stDeviceInfo[iDeviceCount].iDeviceID = pChkPnt->DATA[0];
  }
}

//---------------------------------------------------------------------------
// 동작모드 설정조회 응답메세지( Code:0xB3 , DataLen : 4 ) : 냉난방기 (주기적으로 획득)
// [ PowerMode , SystemMode , Operation , CurTemp , SetTemp , ErrorCode ]
//---------------------------------------------------------------------------
void Protocol_Parser_GetSystemMode(RequestPacket *pChkPnt)
{
  int   iMapID;
  DeviceInfo      *pFindPnt;
  struct tm       timeinfo;

  pFindPnt = DeviceInfo_Search_byID(pChkPnt->SID);
  if (pFindPnt != NULL) {
    pFindPnt->iPower_Mode = pChkPnt->DATA[0];         // 0x00:OFF , 0x01:ON , 0x02:SystemMode
    pFindPnt->iSystemMode = pChkPnt->DATA[1];         // 시스템 모드 0x01:냉방,0x02:난방,0x03:자동,0x04:에러
    pFindPnt->iOperation  = pChkPnt->DATA[2];         // 해당온도에 따라 0x01:동작중 , 0x00:미작동
    pFindPnt->iCurTemperature = pChkPnt->DATA[3] - TEMPERATURE_MARGIN;
    pFindPnt->iSetTemperature = pChkPnt->DATA[4] - TEMPERATURE_MARGIN;
    pFindPnt->iErrorStatus = pChkPnt->DATA[5];
    pFindPnt->iSumTemperature += pFindPnt->iCurTemperature;
    pFindPnt->iGetCount++;
// 기존의 통계관련된 사항에 대한 메모리 및 파일관리 필요함.(2023.03.12)
    if (getLocalTime(&timeinfo)) {
      pFindPnt->iStatTemperature[timeinfo.tm_hour] = pFindPnt->iCurTemperature;
    }
  }
}

//---------------------------------------------------------------------------
// Set SystemMode Response( Code:0xB4 , DataLen : 3 ) : 
// [ PowerMode , SystemMode , SetTemp ]
//---------------------------------------------------------------------------
void Protocol_Parser_SetSystemMode(RequestPacket *pChkPnt)
{
  DeviceInfo      *pFindPnt;

  pFindPnt = DeviceInfo_Search_byID(pChkPnt->SID);
  if (pFindPnt != NULL) {
    pFindPnt->iPower_Mode = pChkPnt->DATA[0];
    if (pFindPnt->iPower_Mode==0x02) {    // SystemMode(2) , ON(1) , OFF(0)
      pFindPnt->iSystemMode = pChkPnt->DATA[1];
      pFindPnt->iSetTemperature = pChkPnt->DATA[2]-TEMPERATURE_MARGIN;   // ????
    }
  }
}

//---------------------------------------------------------------------------
// Get SystemMode Response( Code:0xB8 , DataLen : 6 )
// [ CurTemp , CurHumidity , PowerMode , SystemMode , SetTemp , SetHumidity ]
//---------------------------------------------------------------------------
void Protocol_Parser_GetDehumidifier(RequestPacket *pChkPnt)
{
  DeviceInfo          *pFindPnt;
  RequestDehumidity   *pDehumidity;
  struct tm       timeinfo;

  pFindPnt = DeviceInfo_Search_byID(pChkPnt->SID);
  if (pFindPnt != NULL) {
    pDehumidity = (RequestDehumidity*)&pChkPnt->DATA[0];
    pFindPnt->iPower_Mode = pDehumidity->iPower_Mode;
    pFindPnt->iSystemMode = pDehumidity->iSystemMode;
    pFindPnt->iCurTemperature = pDehumidity->iCurTemp-TEMPERATURE_MARGIN;
    pFindPnt->iCurHumidity = pDehumidity->iCurHumidity;
    pFindPnt->iSetTemperature = pDehumidity->iSetTemp-TEMPERATURE_MARGIN;
    pFindPnt->iSetHumidity = pDehumidity->iSetHumidity;
    pFindPnt->iSumTemperature += pFindPnt->iCurTemperature;
    pFindPnt->iSumHumidity += pFindPnt->iCurHumidity;
    pFindPnt->iGetCount++;
// 기존의 통계관련된 사항에 대한 메모리 및 파일관리 필요함.(2023.03.12)
    if (getLocalTime(&timeinfo)) {
      pFindPnt->iStatTemperature[timeinfo.tm_hour] = pFindPnt->iCurTemperature;
      pFindPnt->iStatHumidity[timeinfo.tm_hour] = pFindPnt->iCurHumidity;
    }
  }
}

//---------------------------------------------------------------------------
// Set SystemMode Response( Code:0xB9 , DataLen : 4 )
// [ PowerMode , SystemMode , SetTemp , SetHumidity ]
//---------------------------------------------------------------------------
void Protocol_Parser_SetDehumidifier(RequestPacket *pChkPnt)
{
  DeviceInfo          *pFindPnt;
  RequestDehumidity   *pDehumidity;

  pFindPnt = DeviceInfo_Search_byID(pChkPnt->SID);
  if (pFindPnt != NULL) {
    pFindPnt->iPower_Mode = pChkPnt->DATA[0];
    if (pFindPnt->iPower_Mode==0x02) {    // SystemMode
      pFindPnt->iSystemMode = pChkPnt->DATA[1];
      pFindPnt->iSetTemperature = pChkPnt->DATA[2]-TEMPERATURE_MARGIN;
      pFindPnt->iSetHumidity = pChkPnt->DATA[3];
    }
  }
}

//-------------------------------------------------------------------------------
// TCP/IP 및 RS485를 통해서 들어오는 데이터에 대한 처리
//-------------------------------------------------------------------------------
void WaterCooller_Receive_Message(int iChannel)
{
  int     i,iLength;
  char    szMsgBuf[2048],szBuffer[1024],szTemp[128],szUseMark[128];
  unsigned short int  iChkCRC;
  RequestPacket       *pChkPnt;
  struct tm       timeinfo;

  while(iRecvCount[iChannel] > 0x04) {
    pChkPnt = (RequestPacket*)&szRecvBuf[iChannel];
    if (pChkPnt->STX==0x02) {
      if ((pChkPnt->LEN+7) <= iRecvCount[iChannel]) {
        iChkCRC = ModRTU_CRC(szRecvBuf[iChannel],pChkPnt->LEN+5);
        if (szRecvBuf[iChannel][pChkPnt->LEN+5]==iChkCRC/0x100 && szRecvBuf[iChannel][pChkPnt->LEN+6]==iChkCRC%0x100) {
          LCD_Display_Recv_Message(0,24,0x02,iRecvCount[iChannel],szRecvBuf[iChannel]);

//          Display_DebugMessage(0x00,iChannel,szRecvBuf[iChannel],(pChkPnt->LEN+7));
          memset(szBuffer,0x00,sizeof(szBuffer));
          if (getLocalTime(&timeinfo)) {
            sprintf(szBuffer,"%d-%02d-%02d %02d:%02d:%02d",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);  
          }
          sprintf(szTemp,"    [%s]   >> Protocol Msg : ID=%02X , CMD=%02X",szBuffer,pChkPnt->SID,pChkPnt->CMD);
          Serial.println(szTemp);                           // 수신된 메세지가 정상적으로 파싱되었다는것을 출력      
          if (0x11<=pChkPnt->CMD && pChkPnt->CMD<=0x19) { // 0x11~0x19 : 485 Send
            SendMessage_ToDevice((unsigned char*)pChkPnt,(pChkPnt->LEN+7));
          } else if (0x91<=pChkPnt->CMD && pChkPnt->CMD<=0x99) { // 0x91~0x99 : TCP Send
            SendMessage_ToServer((unsigned char*)pChkPnt,(pChkPnt->LEN+7));
						if (pChkPnt->CMD==0x93) Protocol_Parser_GetSystemMode(pChkPnt);
						if (pChkPnt->CMD==0x94) Protocol_Parser_SetSystemMode(pChkPnt);
						if (pChkPnt->CMD==0x98) Protocol_Parser_GetDehumidifier(pChkPnt);
          } else if (pChkPnt->CMD==0x21) {                // 0x21 : App 에서의 ID 요청
          } else if (pChkPnt->CMD==0x22) {                // 0x22 : App 에서의 Statistics Data 요청
          } else if (pChkPnt->CMD==0x23) {                // 0x23 : App 에서의 시스템(통신모듈) 시간 설정
          } else if (pChkPnt->CMD==0x2F) {                // 0x2F : App 에서의 시스템 재시작 요청
          } else if (pChkPnt->CMD==0xB1) {                // 0x31 : Get Slave ID , 장비의 주기적인 스캔메세지에 대한 처리를 이곳에서 해야한다.     
            Protocol_Parser_GetID(pChkPnt);
          } else if (pChkPnt->CMD==0xB2) {                // 0x32 : Set Slave ID
            Protocol_Parser_SetID(pChkPnt);
          } else if (pChkPnt->CMD==0xB3) {                // 0x33 : Get System Mode (주기적으로 스캔하여 데이터 저장 : 통계용)
            Protocol_Parser_GetSystemMode(pChkPnt);
          } else if (pChkPnt->CMD==0xB4) {                // 0x34 : Set System Mode
            Protocol_Parser_SetSystemMode(pChkPnt);
          } else if (pChkPnt->CMD==0xB5) {                // 0x35 : 시간/온도 설정확인
          } else if (pChkPnt->CMD==0xB6) {                // 0x36 : 시간/온도 설정변경
          } else if (pChkPnt->CMD==0xB7) {                // 0x37 : 시스템 시간변경
          } else if (pChkPnt->CMD==0xB8) {                // 0x38 : 제습기 설정조회 (주기적으로 스캔하여 데이터 저장 : 통계용)
            Protocol_Parser_GetDehumidifier(pChkPnt); 
          } else if (pChkPnt->CMD==0xB9) {                // 0x39 : 제습기 설정변경
          }
        }
        iRecvCount[iChannel] = iRecvCount[iChannel] - (pChkPnt->LEN+7);
        memcpy(szRecvBuf[iChannel],(unsigned char*)&szRecvBuf[iChannel][(pChkPnt->LEN+7)],iRecvCount[iChannel]);        
      } else {
        return;
      }
    } else {
      iRecvCount[iChannel] = iRecvCount[iChannel] - 1;
      memcpy(szRecvBuf[iChannel],(unsigned char*)&szRecvBuf[iChannel][1],iRecvCount[iChannel]);
    }
  }
}
