// SPI_Logger.cpp : Defines the entry point for the console application.
//  Copyright(C) 2017  SwK (123swk123@gmail.com)
//
//  This program is free software : you can redistribute it and / or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"

#include <stdint.h>
#include <windows.h>
//#include <dbt.h>
#include <conio.h>

#include "../lib/cyusbserial/CyUSBSerial.h"

//Varible to capture return values from USB Serial API 
CY_RETURN_STATUS cyReturnStatus;

//CY_DEVICE_INFO provides additional details of each device such as product Name, serial number etc..
CY_DEVICE_INFO cyDeviceInfo, cyDeviceInfoList[16];

//Variables used by application
UINT8 cyNumDevices;
unsigned char deviceID[16];

int FindDeviceAtSCB0()
{
  CY_VID_PID cyVidPid;

  cyVidPid.vid = 0x04b4; //Defined as macro
  cyVidPid.pid = 0x0004; //Defined as macro

                      //Array size of cyDeviceInfoList is 16 
  cyReturnStatus = CyGetDeviceInfoVidPid(cyVidPid, deviceID, (PCY_DEVICE_INFO)&cyDeviceInfoList, &cyNumDevices, 16);

  int deviceIndexAtSCB0 = -1;
  for (int index = 0; index < cyNumDevices; index++) {
    printf("\nNumber of interfaces: %d\n \
                Vid: 0x%X \n\
                Pid: 0x%X \n\
                Serial name is: %s\n\
                Manufacturer name: %s\n\
                Product Name: %s\n\
                SCB Number: 0x%X \n\
                Device Type: %d \n\
                Device Class: %d\n\n\n",
      cyDeviceInfoList[index].numInterfaces,
      cyDeviceInfoList[index].vidPid.vid,
      cyDeviceInfoList[index].vidPid.pid,
      cyDeviceInfoList[index].serialNum,
      cyDeviceInfoList[index].manufacturerName,
      cyDeviceInfoList[index].productName,
      cyDeviceInfoList[index].deviceBlock,
      cyDeviceInfoList[index].deviceType[0],
      cyDeviceInfoList[index].deviceClass[0]);

    // Find the device at device index at SCB0
    if (cyDeviceInfoList[index].deviceBlock == SerialBlock_SCB0)
    {
      deviceIndexAtSCB0 = index;
    }
  }
  return deviceIndexAtSCB0;
}


volatile bool bRun = true;
volatile CY_HANDLE hSPI;
volatile uint8_t u8TxBuff[100], u8RxBuff[2][4096];
volatile uint8_t u8BuffPg = 0;
volatile UINT32 u32ReadLen;

DWORD WINAPI thrSPI_Handler(LPVOID lpParameter)
{
  CY_SPI_CONFIG cfgSPI;
  CY_RETURN_STATUS rtnStatus;

  rtnStatus = CyGetSpiConfig(hSPI, &cfgSPI);

  cfgSPI.frequency = 800000;
  cfgSPI.isMaster = 0;
  cfgSPI.dataWidth = 8;

  rtnStatus = CySetSpiConfig(hSPI, &cfgSPI);

  //rtnStatus = CyResetDevice(hSPI);

  CY_DATA_BUFFER myTxData;
  CY_DATA_BUFFER myRxData;
  
  myTxData.buffer = (UCHAR*)u8TxBuff;
  myRxData.buffer = (UCHAR*)&u8RxBuff[u8BuffPg][0];
  myRxData.length = 900 * 1; myTxData.length = 16;

  puts("start....");

  for (int i = 0; /*i < 100*/bRun; i++)
  {
    //printf("len: %d, ", myRxData.length);
    rtnStatus = CySpiReadWrite(hSPI, &myRxData, NULL, 5000);

    if (rtnStatus == CY_SUCCESS)
    {
      u32ReadLen = myRxData.transferCount;
      u8BuffPg = u8BuffPg ^ 1;
      myRxData.buffer = (UCHAR*)&u8RxBuff[u8BuffPg][0];

      //printf("RxCnt:%d\r\n", u32ReadLen);
      
    }
    else
    {
      u32ReadLen = 0;
      //printf("loop cnt %d :- ERROR\r\n", i);
      //myRxData.length = max(2, myRxData.length / 2);
    }
  }

  return 0;
}


int main()
{
  UINT8 u8DeviceCount;
  CY_DEVICE_INFO test;

  CyGetListofDevices(&u8DeviceCount);
  
  printf("Device Count: %d\n", u8DeviceCount);

  UINT8 u8devSPInum = u8DeviceCount;
  UINT8 u8infNum;
  for (UINT8 i = 0; i < u8DeviceCount; i++)
  {
    CyGetDeviceInfo(i, &test);

    for (UINT8 j = 0; j < test.numInterfaces; j++)
    {
      if (test.deviceType[j] == CY_TYPE_SPI)
      {
        u8infNum = j;
        u8devSPInum = i;
        i = u8DeviceCount;
        break;
      }
    }
  }
  
  if (u8devSPInum == u8DeviceCount)
  {
    //no match found
  }
  else
  {
    
    CY_SPI_CONFIG cfgSPI;
    CY_RETURN_STATUS rtnStatus;

    rtnStatus = CyOpen(u8devSPInum, u8infNum, (CY_HANDLE*) &hSPI);
    rtnStatus = CyResetDevice(hSPI); Sleep(1000);
    rtnStatus = CyClose(hSPI);
    rtnStatus = CyOpen(u8devSPInum, u8infNum, (CY_HANDLE*) &hSPI);

    FILE *hfout = fopen("test.log", "wb");

    HANDLE hThread = CreateThread(NULL, 0, thrSPI_Handler, 0, 0, 0);

    int i = 0;
    while (WaitForSingleObject(hThread, 100) == WAIT_TIMEOUT)
    {
      if (u32ReadLen)
      {
        fwrite((const void *)&u8RxBuff[!u8BuffPg][0], 1, u32ReadLen, hfout);
        fprintf(hfout, "\ncount: %d\n", i);

        for (size_t i = 0; i < u32ReadLen; i++)
        {
          printf("%02X ", u8RxBuff[!u8BuffPg][i]);
        }
        printf("\ncount: %d\n", i++);
        u32ReadLen = 0;
      }

      if (_kbhit())
      {
        bRun = false;
      }
    }

    rtnStatus = CyClose(hSPI);

    CloseHandle(hThread);

    fclose(hfout);
    

    //delete myTxData.buffer;
    //delete myRxData.buffer;
  }
  return 0;
}

