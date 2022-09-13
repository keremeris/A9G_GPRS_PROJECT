

#include <string.h>
#include <stdio.h>
#include <api_os.h>
#include <api_event.h>
#include <api_socket.h>
#include <api_network.h>
#include <api_debug.h>
#include <api_gps.h>
#include "gps_parse.h"
#include "math.h"
#include "gps.h"
#include "api_info.h"

#include "stdbool.h"
#include "stdint.h"
#include "stdlib.h"
#include "api_sms.h"
#include "api_hal_uart.h"
#include "api_hal_gpio.h"
#include "api_hal_pm.h"
/*******************************************************************/
/////////////////////////socket configuration////////////////////////

#define DNS_DOMAIN  "121.200.60.194"
#define SERVER_PORT 8082
#define RECEIVE_BUFFER_MAX_LENGTH 200
/*******************************************************************/
///////////////////////SMS config/////////////////////////////////////////
#define TEST_PHONE_NUMBER "8801711540216"
const uint8_t unicodeMsg[] = {0x00, 0x61, 0x00, 0x61, 0x00, 0x61, 0x6d, 0x4b, 0x8b, 0xd5, 0x77, 0xed, 0x4f, 0xe1}; //unicode:aaa测试短信
const uint8_t gbkMsg[]     = {0x62, 0x62, 0x62, 0xB0, 0xA1, 0xB0, 0xA1, 0xB0, 0xA1, 0xB0, 0xA1, 0x63, 0x63, 0x63 };//GBK    :bbb啊啊啊啊ccc
const uint8_t utf8Msg[]    = "hello,this is our test sms";//Cause the encoding format of this file(sms.c) is UTF-8
                            // UTF-8 Bytes:75 74 66 2D 38 E6 B5 8B E8 AF 95 E7 9F AD E4 BF A1 
////////////////////////////////////////////////////////////////////////////////////////////////////////


#define MAIN_TASK_STACK_SIZE    (2048 * 2)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME          "Socket Test Main Task"

#define TEST_TASK_STACK_SIZE    (2048 * 2)
#define TEST_TASK_PRIORITY      1
#define TEST_TASK_NAME          "Socket Test Task"

#define GPIO_TASK_STACK_SIZE    (2048 * 2)
#define GPIO_TASK_PRIORITY      2
#define GPIO_TASK_NAME          "GPIO TASK"

static HANDLE projectTaskHandle = NULL;

int socketFd = -1;
uint8_t buffer[RECEIVE_BUFFER_MAX_LENGTH];
HANDLE sem = NULL;
int errorCode = 0;
bool didFlag = false;
//GPS_START

bool isGpsOn = true;
bool flag = false;
uint8_t x1 = 0b11111111;
uint8_t x2 = 0b10111111;
uint8_t x3 = 0b10111011;
uint8_t x4 = 0b11111111;
//GPS_END

void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
        case API_EVENT_ID_NO_SIMCARD:
            Trace(10,"!!NO SIM CARD%d!!!!",pEvent->param1);
            break;
        case API_EVENT_ID_GPS_UART_RECEIVED:
            if(didFlag == true) break;
            Trace(1,"received GPS data,length:%d, data:%s,flag:%d",pEvent->param1,pEvent->pParam1,flag);
            GPS_Update(pEvent->pParam1,pEvent->param1);
            break;
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            Trace(2,"network register success");
            Network_StartAttach();
            flag = true;
            break;

        case API_EVENT_ID_NETWORK_ATTACHED:
            Trace(2,"network attach success");
            Network_PDP_Context_t context = {
                .apn        ="cmnet",
                .userName   = ""    ,
                .userPasswd = ""
            };
            Network_StartActive(context);
            break;

        case API_EVENT_ID_NETWORK_ACTIVATED:
            Trace(2,"network activate success");
            // if(sem)
            //     OS_ReleaseSemaphore(sem);
            sem = 1;
            break;

        case API_EVENT_ID_SOCKET_CONNECTED:
            Trace(2,"event connect");
            // if(sem)
            //     OS_ReleaseSemaphore(sem);
            sem = 1;
            break;

        case API_EVENT_ID_SOCKET_SENT:
            // if(sem)
            //     OS_ReleaseSemaphore(sem);
            sem = 1;
            break;
        case API_EVENT_ID_SOCKET_RECEIVED:
        {
            if(didFlag == true) break;
            char *revData ;
            int fd = pEvent->param1;
            int length = pEvent->param2>RECEIVE_BUFFER_MAX_LENGTH?RECEIVE_BUFFER_MAX_LENGTH:pEvent->param2;
            memset(buffer,0,sizeof(buffer));
            length = Socket_TcpipRead(fd,buffer,length);
            revData = buffer;

            Trace(2,"socket %d received %d bytes data:%s",fd,length,revData);
           
            if((strcmp(revData,"PowerOff")==0) | (strcmp(revData,"PowerOn")==0))
            {
                GPIO_Set(GPIO_PIN25,GPIO_LEVEL_HIGH);
            }

            break;
        }
        case API_EVENT_ID_SOCKET_CLOSED:
        {
            int fd = pEvent->param1;
            Trace(2,"socket %d closed",fd);
            // if(sem)
            //     OS_ReleaseSemaphore(sem);
            sem = 1;
            break;
        }
        case API_EVENT_ID_SOCKET_ERROR:
        {
            int fd = pEvent->param1;
            Trace(2,"socket %d error occurred,cause:%d",fd,pEvent->param2);
            errorCode = pEvent->param2;
            // if(sem)
            //     OS_ReleaseSemaphore(sem);
            sem = 1;
            break;
        }
        case API_EVENT_ID_SMS_SENT:
        {
            Trace(2,"Send Message Success");
            break;
        }
        case API_EVENT_ID_SMS_RECEIVED:
            if(didFlag == true) break;
            Trace(2,"received message");
            SMS_Encode_Type_t encodeType = pEvent->param1;
            uint32_t contentLength = pEvent->param2;
            uint8_t* header = pEvent->pParam1;
            uint8_t* content = pEvent->pParam2;
            GPIO_Set(GPIO_PIN25,GPIO_LEVEL_HIGH);
            Trace(2,"message header:%s",header);
            Trace(2,"message content length:%d",contentLength);

            if(encodeType == SMS_ENCODE_TYPE_ASCII)
            {
                Trace(2,"message content:%s",content);
               // UART_Write(UART1,content,contentLength);
            }
            else
            {
                uint8_t tmp[500];
                memset(tmp,0,500);
                for(int i=0;i<contentLength;i+=2)
                    sprintf(tmp+strlen(tmp),"\\u%02x%02x",content[i],content[i+1]);
                Trace(2,"message content(unicode):%s",tmp);//you can copy this string to http://tool.chinaz.com/tools/unicode.aspx and display as Chinese
                uint8_t* gbk = NULL;
                uint32_t gbkLen = 0;
                if(!SMS_Unicode2LocalLanguage(content,contentLength,CHARSET_CP936,&gbk,&gbkLen))
                    Trace(10,"convert unicode to GBK fail!");
                else
                {
                    memset(tmp,0,500);
                    for(int i=0;i<gbkLen;i+=2)
                        sprintf(tmp+strlen(tmp),"%02x%02x ",gbk[i],gbk[i+1]);
                    Trace(2,"message content(GBK):%s",tmp);//you can copy this string to http://m.3158bbs.com/tool-54.html# and display as Chinese
                   // UART_Write(UART1,gbk,gbkLen);//use serial tool that support GBK decode if have Chinese, eg: https://github.com/Neutree/COMTool
                }
                OS_Free(gbk);
            }
            break;
        case API_EVENT_ID_SMS_LIST_MESSAGE:
        {
            SMS_Message_Info_t* messageInfo = (SMS_Message_Info_t*)pEvent->pParam1;
            Trace(1,"message header index:%d,status:%d,number type:%d,number:%s,time:\"%u/%02u/%02u,%02u:%02u:%02u+%02d\"", messageInfo->index, messageInfo->status,
                                                                                        messageInfo->phoneNumberType, messageInfo->phoneNumber,
                                                                                        messageInfo->time.year, messageInfo->time.month, messageInfo->time.day,
                                                                                        messageInfo->time.hour, messageInfo->time.minute, messageInfo->time.second,
                                                                                        messageInfo->time.timeZone);
            Trace(1,"message content len:%d,data:%s",messageInfo->dataLen,messageInfo->data);
         //   UART_Write(UART1, messageInfo->data, messageInfo->dataLen);//use serial tool that support GBK decode if have Chinese, eg: https://github.com/Neutree/COMTool
           // UART_Write(UART1,"\r\n\r\n",4);
            //need to free data here
            OS_Free(messageInfo->data);
            break;
        }
        case API_EVENT_ID_SMS_ERROR:
            Trace(10,"SMS error occured! cause:%d",pEvent->param1);
        default:
            break;
    }
}
///////NETWORK FUNCTION////////////////////////////////////////////////////
void CreateSem(HANDLE* sem_)
{
    *sem_ = 0;
    // *sem = OS_CreateSemaphore(0);
}

void WaitSem(HANDLE* sem_)
{
    // OS_WaitForSemaphore(*sem,OS_WAIT_FOREVER);
    // OS_DeleteSemaphore(*sem);
    // *sem = NULL;
    while(*sem_ == 0)
        OS_Sleep(1);
    *sem_ = 0;
}

bool Connect()
{
    memset(buffer,0,sizeof(buffer));
    if(DNS_GetHostByName2(DNS_DOMAIN,(char*)buffer) != 0)
        return false;
    Trace(2,"DNS,domain:%s,ip:%s,strlen(ip):%d",DNS_DOMAIN,buffer,strlen(buffer));
    CreateSem(&sem);
    socketFd = Socket_TcpipConnect(TCP,buffer,SERVER_PORT);
    Trace(2,"connect tcp server,socketFd:%d",socketFd);
    WaitSem(&sem);
    Trace(2,"connect end");
    if(errorCode != 0)
    {
        errorCode = 0;
        Trace(2,"error ocurred");
        return false;
    }
    return true;
}
bool Write(uint8_t* data, uint16_t len)
{
    Trace(2,"Write");
    CreateSem(&sem);
    int ret = Socket_TcpipWrite(socketFd,data,len);
    if(ret <= 0)
    {
        Trace(2,"socket write fail:%d",ret);
        return false;
    }    
    Trace(2,"### socket %d send %d bytes data to server:%s,ret:%d",socketFd, len, data,ret);
    WaitSem(&sem);
    Trace(2,"### write end");
    if(errorCode != 0)
    {
        errorCode = 0;
        Trace(2,"error ocurred");
        return false;
    }
    return true;
}

bool Close()
{
    CreateSem(&sem);
    Socket_TcpipClose(socketFd);
    WaitSem(&sem);
    return true;
}
//____________________________________________________________________________//
//////////////////SMS FUNC?////////////////////////////////////////////////////////
void UartInit()
{
    UART_Config_t config = {
        .baudRate = UART_BAUD_RATE_115200,
        .dataBits = UART_DATA_BITS_8,
        .stopBits = UART_STOP_BITS_1,
        .parity   = UART_PARITY_NONE,
        .rxCallback = NULL,
    };
    UART_Init(UART1,config);
}

void SMSInit()
{
    if(!SMS_SetFormat(SMS_FORMAT_TEXT,SIM0))
    {
        Trace(1,"sms set format error");
        return;
    }
    SMS_Parameter_t smsParam = {
        .fo = 17 ,
        .vp = 167,
        .pid= 0  ,
        .dcs= 8  ,//0:English 7bit, 4:English 8 bit, 8:Unicode 2 Bytes
    };
    if(!SMS_SetParameter(&smsParam,SIM0))
    {
        Trace(1,"sms set parameter error");
        return;
    }
    if(!SMS_SetNewMessageStorage(SMS_STORAGE_SIM_CARD))
    {
        Trace(1,"sms set message storage fail");
        return;
    }
}
void SendSmsUnicode()
{
    Trace(1,"sms start send unicode message");
    if(!SMS_SendMessage(TEST_PHONE_NUMBER,unicodeMsg,sizeof(unicodeMsg),SIM0))
    {
        Trace(1,"sms send message fail");
    }
}

void SendSmsGbk()
{
    uint8_t* unicode = NULL;
    uint32_t unicodeLen;

    Trace(1,"sms start send GBK message");

    if(!SMS_LocalLanguage2Unicode(gbkMsg,sizeof(gbkMsg),CHARSET_CP936,&unicode,&unicodeLen))
    {
        Trace(1,"local to unicode fail!");
        return;
    }
    if(!SMS_SendMessage(TEST_PHONE_NUMBER,unicode,unicodeLen,SIM0))
    {
        Trace(1,"sms send message fail");
    }
    OS_Free(unicode);
}

void SendUtf8()
{
    uint8_t* unicode = NULL;
    uint32_t unicodeLen;

    Trace(1,"sms start send UTF-8 message");

    if(!SMS_LocalLanguage2Unicode(utf8Msg,strlen(utf8Msg),CHARSET_UTF_8,&unicode,&unicodeLen))
    {
        Trace(1,"local to unicode fail!");
        return;
    }
    if(!SMS_SendMessage(TEST_PHONE_NUMBER,unicode,unicodeLen,SIM0))
    {
        Trace(1,"sms send message fail");
    }
    OS_Free(unicode);
}


void SendSMS()
{
    // SendSmsUnicode();
    // SendSmsGbk();
    SendUtf8();
}

//_________________________________________________________________________________//
/////////////////////GPIO Init//////////////////////////////////////////////////////
void GPIO_Format()
{
    GPIO_config_t gpioin1 = {
        .mode         = GPIO_MODE_INPUT,
        .pin          = GPIO_PIN2,
        .defaultLevel = GPIO_LEVEL_LOW
    };
    
  
    GPIO_config_t gpioin2 = {
        .mode         = GPIO_MODE_INPUT,
        .pin          = GPIO_PIN3,
        .defaultLevel = GPIO_LEVEL_LOW
    };
   
    GPIO_config_t gpioin3 = {
        .mode         = GPIO_MODE_INPUT,
        .pin          = GPIO_PIN4,
        .defaultLevel = GPIO_LEVEL_LOW
    };
  
    GPIO_config_t gpioin4 = {
        .mode         = GPIO_MODE_INPUT,
        .pin          = GPIO_PIN5,
        .defaultLevel = GPIO_LEVEL_LOW
    };
 
   
    GPIO_config_t gpioin5 = {
        .mode         = GPIO_MODE_INPUT,
        .pin          = GPIO_PIN6,
        .defaultLevel = GPIO_LEVEL_LOW
    };

  
    GPIO_config_t gpioout1 = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin          = GPIO_PIN25,
        .defaultLevel = GPIO_LEVEL_LOW
    };
    PM_PowerEnable(POWER_TYPE_VPAD,true);
    GPIO_Init(gpioin1);
    GPIO_Init(gpioin2);
    GPIO_Init(gpioin3);
    GPIO_Init(gpioin4);
    GPIO_Init(gpioin5);
    GPIO_Init(gpioout1);
    //Trace(5,"GPIO_IN_1 pin Init:%s",GPIO_Init(gpioin1) ? "true" : "false");
    //Trace(5,"GPIO_IN_2 pin Init:%s",GPIO_Init(gpioin2) ? "true" : "false");
   // Trace(5,"GPIO_IN_1 pin Init:%s", GPIO_Init(gpioin3) ? "true" : "false");
  //  Trace(5,"GPIO_IN_4 pin Init:%s",GPIO_Init(gpioin4) ? "true" : "false");
  //  Trace(5,"GPIO_IN_5 pin Init:%s",GPIO_Init(gpioin5) ? "true" : "false");
  //  Trace(5,"GPIO_OUT_1 pin Init:%s",GPIO_Init(gpioout1) ? "true" : "false");

}
void projectTask(void* param)
{
    uint8_t imei[16];
    uint8_t buffer_sndData[400];
    char* isFixedStr;
    char* isValidStr;
    int failCount = 0;
    int count = 0;
    int limit = 0;
    WaitSem(&sem);
    while(!flag)
    {
        Trace(1,"wait for gprs regiter complete");
        OS_Sleep(2000);
    }

    GPS_Info_t* gpsInfo = Gps_GetInfo();
    
    GPS_Init();
    GPS_Open(NULL);
    while(gpsInfo->rmc.latitude.value == 0)
        OS_Sleep(1000);
  

    // set gps nmea output interval
    for(uint8_t i = 0;i<5;++i)
    {
        bool ret = GPS_SetOutputInterval(10000);
        Trace(1,"set gps ret:%d",ret);
        if(ret)
            break;
        OS_Sleep(1000);
    }

    // if(!GPS_ClearInfoInFlash())
    //     Trace(1,"erase gps fail");
    
    // if(!GPS_SetQzssOutput(false))
    //     Trace(1,"enable qzss nmea output fail");

    // if(!GPS_SetSearchMode(true,false,true,false))
    //     Trace(1,"set search mode fail");

    // if(!GPS_SetSBASEnable(true))
    //     Trace(1,"enable sbas fail");
    
    if(!GPS_GetVersion(buffer,150))
        Trace(1,"get gps firmware version fail");
    else
        Trace(1,"gps firmware version:%s",buffer);

    // if(!GPS_SetFixMode(GPS_FIX_MODE_LOW_SPEED))
        // Trace(1,"set fix mode fail");

    if(!GPS_SetOutputInterval(1000))
        Trace(1,"set nmea output interval fail");
    
    Trace(1,"GPS_Init ok");
    Trace(2,"sem:%d,%p",(int)sem,(void*)sem);
    Trace(1,"start connect now");
  
    //Network
    Connect();
    //SMS
    SMSInit();
    while(1)
    {
                    
        memset(imei,0,sizeof(imei));
        INFO_GetIMEI(imei);
        Trace(1,"IMEI NUMBER %s",imei);
        Trace(2,"count:%d",count++);
      //  int temp = (int)(gpsInfo->gga.latitude.value/gpsInfo->gga.latitude.scale/100);
        //double latitude = (double)(gpsInfo->gga.latitude.value - temp*gpsInfo->gga.latitude.scale*100)/gpsInfo->gga.latitude.scale/60.0;
       // temp = (int)(gpsInfo->gga.longitude.value/gpsInfo->gga.longitude.scale/100);
        //double longitude = (double)(gpsInfo->gga.longitude.value - temp*gpsInfo->gga.longitude.scale*100)/gpsInfo->gga.longitude.scale/60.0;
        double latitude = (double)((double)(gpsInfo->gga.latitude.value)/(double)(gpsInfo->gga.latitude.scale));
        double longitude = (double)((double)(gpsInfo->gga.longitude.value)/(double)(gpsInfo->gga.longitude.scale));
        char *tmp_str ="470,01,21097,13140#";
        isValidStr = gpsInfo->rmc.valid ? "A" : "V";
        int speed = (int)((double)(gpsInfo->rmc.speed.value)/(double)(gpsInfo->rmc.speed.scale));
        int direct = (int)((double)(gpsInfo->rmc.course.value)/(double)(gpsInfo->rmc.course.scale));
     
        snprintf(buffer_sndData,sizeof(buffer_sndData),"*HQ,%s,V1,%02d%02d%02d,%s,%.4f,N,%s%.4f,E,%d,%d,%02d%02d%02d,%x%x%x%x,%s",imei,gpsInfo->rmc.time.hours,gpsInfo->rmc.time.minutes,gpsInfo->rmc.time.seconds,isValidStr,latitude,"0",longitude,speed,direct,gpsInfo->rmc.date.year,gpsInfo->rmc.date.month,gpsInfo->rmc.date.day,x1,x2,x3,x4,tmp_str);
        
        isFixedStr = buffer_sndData;
        Trace(1,isFixedStr);
        if(failCount == 5)
        {
            Close();
        }
        if(failCount >= 5)
        {
            if(Connect())
                failCount = 0;
            else
                ++failCount;
        }
        else
        {
            if(!Write(isFixedStr,strlen(isFixedStr)))
            {
                ++failCount;
                Trace(2,"write fail");
            }
            Trace(2,"Send Server Success:Data:%s",isFixedStr);
        }
       
        OS_Sleep(5000);
    }
}
void gpioTask(void* param)
{
    bool b = false;
    GPIO_LEVEL statusNow;
    GPIO_Format();
    Trace(5,"GPIO INIT OK");
    while(1)
    {
        if(didFlag == true) break;
  
        Trace(5,"This is GPIO_TASK");
        GPIO_Get(GPIO_PIN6,&statusNow);
        if(statusNow != 0)
        {
            x2 = 0b10111011;
            Trace(5,"Value of X2:%d",x2);
        }
        else
        {
            x2 = 0b10111111;
            Trace(5,"Value of X2:%d",x2);
        }
        GPIO_Get(GPIO_PIN5,&statusNow);
        if(statusNow != 0)
        {
            x4 = 0b11111011;
            Trace(5,"Value of X4:%d",x4);
        }
        else
        {
            x4 = 0b11111111;
            Trace(5,"Value of X4:%d",x4);
        }
        GPIO_Get(GPIO_PIN4,&statusNow);
        if(statusNow != 0)
        {
            x3 = 0b10111011;
            Trace(5,"Value of X3:%x",x3);
        }
        else
        {
            x3 = 0b10111111;
            Trace(5,"Value of X3:%x",x3);
        }
         
        GPIO_Get(GPIO_PIN25,&statusNow);
        Trace(5,"Status of GPIO 25:%d",statusNow);
       
       // Trace(5,"Status of GPIO 25 :%s", b ? "true" : "false");
        OS_Sleep(1000);
    }

}
void socket_MainTask(void *pData)
{
    API_Event_t* event=NULL;

    CreateSem(&sem);
    OS_CreateTask(projectTask,
        NULL, NULL, TEST_TASK_STACK_SIZE, TEST_TASK_PRIORITY, 0, 0, TEST_TASK_NAME);
    OS_CreateTask(gpioTask,
        NULL, NULL, GPIO_TASK_STACK_SIZE, GPIO_TASK_PRIORITY, 0, 0, GPIO_TASK_NAME);
    while(1)
    {
        if(OS_WaitEvent(projectTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
        {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}

void project_Main(void)
{
    projectTaskHandle = OS_CreateTask(socket_MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&projectTaskHandle);
}

