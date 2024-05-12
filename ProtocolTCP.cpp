#include <stdio.h>
#include <Ticker.h>
#include "Arduino.h"
#include "UserDefine.h"
#include "WaterCool.h"
#include "FS.h"
#include "SPIFFS.h"
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
extern int                  isSFlash;

//---------------------------------------------------------------------------
// ( Code:0x21 , DataLen : 0 ) 
//---------------------------------------------------------------------------
void Protocol_Parser_GetEntireID(RequestPacket *pChkPnt)
{
  int             iCount = 0;
  unsigned short int    iChkCRC;
  RequestPacket         stSendMsg;

  stSendMsg.STX = 0x02;
  stSendMsg.SID = pChkPnt->SID;
  stSendMsg.CMD = 0x21 + 0x80;
  for(int i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==0x01) {
      stSendMsg.DATA[iCount*10+0] = stDeviceInfo[i].iDeviceID;
      stSendMsg.DATA[iCount*10+1] = stDeviceInfo[i].iDeviceType;
      stSendMsg.DATA[iCount*10+2] = stDeviceInfo[i].iPower_Mode;
      stSendMsg.DATA[iCount*10+3] = stDeviceInfo[i].iSystemMode;
      stSendMsg.DATA[iCount*10+4] = stDeviceInfo[i].iOperation;
      stSendMsg.DATA[iCount*10+5] = stDeviceInfo[i].iCurTemperature + TEMPERATURE_MARGIN;
      stSendMsg.DATA[iCount*10+6] = stDeviceInfo[i].iCurHumidity;
      stSendMsg.DATA[iCount*10+7] = stDeviceInfo[i].iSetTemperature + TEMPERATURE_MARGIN;
      stSendMsg.DATA[iCount*10+8] = stDeviceInfo[i].iSetHumidity;
      stSendMsg.DATA[iCount*10+9] = stDeviceInfo[i].iValveStatus;
      iCount = iCount + 1;
    }
  }
  stSendMsg.LEN = iCount * 10;
  iChkCRC = ModRTU_CRC((unsigned char*)&stSendMsg,stSendMsg.LEN+5);
  stSendMsg.DATA[stSendMsg.LEN+0] = iChkCRC / 0x100;
  stSendMsg.DATA[stSendMsg.LEN+1] = iChkCRC % 0x100;
  SendMessage_ToServer((uint8_t*)&stSendMsg,stSendMsg.LEN+7);
}

//---------------------------------------------------------------------------
// ( Code:0x22 , DataLen : 0 ) - 통계자료 확인
//---------------------------------------------------------------------------
int CalculateDay_byYearMonth(int iYear,int iMonth)
{
  int   iLastDay;
  int   iDays[12] = {31,28,31,30,31,30,31,31,30,31,30,31 };

  iLastDay = iDays[iMonth-1];
  if (iMonth==2) {
    iLastDay += (iYear%4==0 && iYear%100!=0 && iYear%400==0) ? 1 : 0;
  }
  return(iLastDay);
}

//---------------------------------------------------------------------------
// Set DateTime( Code:0x22 , DataLen : 7 )
//---------------------------------------------------------------------------
void Protocol_Parser_GetStatistics(RequestPacket *pChkPnt)
{
  unsigned short int    iChkCRC;
  RequestStatistics     *pChkStat;
  RequestPacket         stSendMsg;
  char    szBuffer[1024],szFileName[128];
  File    fp;

  if (pChkPnt != NULL) {
    pChkStat = (RequestStatistics*)&pChkPnt->DATA[0];
    if (pChkStat != NULL && isSFlash==true) {
      int Days = CalculateDay_byYearMonth((int)(pChkPnt->DATA[1]+pChkPnt->DATA[2]*0x100),(int)(pChkPnt->DATA[3]));
      stSendMsg.STX = 0x02;
      stSendMsg.SID = pChkPnt->SID;
      stSendMsg.CMD = 0xA2;
      stSendMsg.LEN = 1 + (Days*24);
			stSendMsg.DATA[0] = pChkStat->iDataType;
      if (pChkStat->iDataType==0x01) {
        sprintf(szFileName,"T%02d_%04d%02d",pChkPnt->SID,pChkStat->Year,pChkStat->Month);  
        fp = SPIFFS.open(szFileName, "r");     
        if (!fp) {
          fp.seek(0, SeekSet);
          fp.readBytes((char*)&stSendMsg.DATA[1],stSendMsg.LEN);
          fp.close();
        }
      } else if (pChkStat->iDataType==0x02) {
        sprintf(szFileName,"H%02d_%04d%02d",pChkPnt->SID,pChkStat->Year,pChkStat->Month);  
        fp = SPIFFS.open(szFileName, "r");     
        if (!fp) {
          fp.seek(0, SeekSet);
          fp.readBytes((char*)&stSendMsg.DATA[1],stSendMsg.LEN);
          fp.close();
        }
      }
      iChkCRC = ModRTU_CRC((unsigned char*)&stSendMsg,stSendMsg.LEN+5);
      stSendMsg.DATA[stSendMsg.LEN+0] = iChkCRC / 0x100;
      stSendMsg.DATA[stSendMsg.LEN+1] = iChkCRC % 0x100;
      SendMessage_ToServer((uint8_t*)&stSendMsg,stSendMsg.LEN+7);      
    }
  }
}

//---------------------------------------------------------------------------
// Set DateTime( Code:0x23 , DataLen : 7 )
//---------------------------------------------------------------------------
void Protocol_Parser_SetDateTime(RequestPacket *pChkPnt)
{
  unsigned short int    iChkCRC;
  RequestDateTime       *pChkDate;
  RequestPacket         stSendMsg;

  if (pChkPnt != NULL) {
    pChkDate = (RequestDateTime*)&pChkPnt->DATA[0];
    if (pChkDate != NULL) {
      if (pChkDate->Hour!=0 && pChkDate->Minute!=0 && pChkDate->Second!=0) {
//				RTC_SetDateTime(pChkDate);
			}
    }
    stSendMsg.STX = 0x02;
    stSendMsg.SID = 0x00;
    stSendMsg.CMD = 0xA3;
    stSendMsg.LEN = 0x07;
    memcpy((char*)&stSendMsg.DATA[0],(char*)&pChkPnt->DATA[0],0x07);
    iChkCRC = ModRTU_CRC((unsigned char*)&stSendMsg,stSendMsg.LEN+5);
    stSendMsg.DATA[stSendMsg.LEN+0] = iChkCRC / 0x100;
    stSendMsg.DATA[stSendMsg.LEN+1] = iChkCRC % 0x100;
		SendMessage_ToServer((unsigned char*)&stSendMsg,stSendMsg.LEN+7);
  }  
}

//-------------------------------------------------------------------------------
// TCP/IP 및 RS485를 통해서 들어오는 데이터에 대한 처리
//-------------------------------------------------------------------------------
void Protocol_TCP_Receive_Message(int iChannel)
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
          memset(szBuffer,0x00,sizeof(szBuffer));
          if (getLocalTime(&timeinfo)) {
            sprintf(szBuffer,"%d-%02d-%02d %02d:%02d:%02d",1900+timeinfo.tm_year,timeinfo.tm_mon,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);  
            sprintf(szTemp,"    [%s]   >> Protocol Msg : ID=%02X , CMD=%02X",szBuffer,pChkPnt->SID,pChkPnt->CMD);
            Serial.println(szTemp);                           // 수신된 메세지가 정상적으로 파싱되었다는것을 출력     
          }
          if (0x11<=pChkPnt->CMD && pChkPnt->CMD<=0x19) { // 0x11~0x19 : App Server Recv (to 485)
            SendMessage_ToDevice((unsigned char*)pChkPnt,(pChkPnt->LEN+7));
          } else if (0x91<=pChkPnt->CMD && pChkPnt->CMD<=0x99) { // 0x91~0x99 : App Server Send (to TCP)
            SendMessage_ToServer((unsigned char*)pChkPnt,(pChkPnt->LEN+7));
          } else if (pChkPnt->CMD==0x21) {                // 0x21 : App 에서의 ID 요청
            Protocol_Parser_GetEntireID(pChkPnt);
          } else if (pChkPnt->CMD==0x22) {                // 0x22 : App 에서의 Statistics Data 요청
            Protocol_Parser_GetStatistics(pChkPnt);
          } else if (pChkPnt->CMD==0x23) {                // 0x23 : App 에서의 시스템(통신모듈) 시간 설정
            Protocol_Parser_SetDateTime(pChkPnt);
          } else if (pChkPnt->CMD==0x2F) {                // 0x2F : App 에서의 시스템 재시작 요청
            if (pChkPnt->SID==0x2F) ESP.restart();          
          } else if (pChkPnt->CMD==0xB1) {                // 0x31 : Get Slave ID , 장비의 주기적인 스캔메세지에 대한 처리를 이곳에서 해야한다.     
//            Protocol_Parser_GetID(pChkPnt);
          } else if (pChkPnt->CMD==0xB2) {                // 0x32 : Set Slave ID
          } else if (pChkPnt->CMD==0xB3) {                // 0x33 : Get System Mode (주기적으로 스캔하여 데이터 저장 : 통계용)
//            Protocol_Parser_GetSystemMode(pChkPnt);
          } else if (pChkPnt->CMD==0xB4) {                // 0x34 : Set System Mode
          } else if (pChkPnt->CMD==0xB5) {                // 0x35 : 시간/온도 설정확인
          } else if (pChkPnt->CMD==0xB6) {                // 0x36 : 시간/온도 설정변경
          } else if (pChkPnt->CMD==0xB7) {                // 0x37 : 시스템 시간변경
          } else if (pChkPnt->CMD==0xB8) {                // 0x38 : 제습기 설정조회 (주기적으로 스캔하여 데이터 저장 : 통계용)
//            Protocol_Parser_GetDehumidifier(pChkPnt); 
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
