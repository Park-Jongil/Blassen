#ifndef WATERCOOL_FUNCTION_H
#define WATERCOOL_FUNCTION_H
  void SlaveDevice_ScanbyTime();
  void SlaveDevice_GetDeviceID(int iID,int iCmd);
  void SlaveDevice_SetDeviceID(int iID,int iCmd,int iSID);
  void SlaveDevice_GetStatusbyTime();  
  void SlaveDevice_SetSystemMode(int iDeviceID,int iCmd,int iCmdOP,int iSystemMode,int iSetTemp);
  void SlaveDevice_SetSystemSetting(int iDeviceID,int iCmd,RequestTempSetting *pChkPnt);  
  void SlaveDevice_SetSystemTime();  
  void SlaveDevice_SetSystemMode_Humidify(int iDeviceID,int iCmd,int iPowerMode,int iSystemMode,int iSetTemp,int iSetHudm);
  
  void Protocol_Parser_SetSystemMode(int iDeviceID,int iPowerMode,int iSystemMode,int iSetTemp);
  void Protocol_Parser_GetID(RequestPacket *pChkPnt);
  void Protocol_Parser_GetSystemMode(RequestPacket *pChkPnt);
  void WaterCooller_Receive_Message(int iChannel);
#endif
