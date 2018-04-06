#ifndef __IAP_H__
#define __IAP_H__
#include <stdint.h>  

typedef  void (*iapfun)(void);				//定义一个函数类型的参数.

#define FLASH_APP1_ADDR		( 0x08018000 )  //第一个应用程序起始地址(存放在FLASH)
											
											
//FLASH起始地址
#define STM32_FLASH_BASE 	( 0x08000000 ) 	//STM32 FLASH的起始地址
#define STM_SECTOR_SIZE		( 2048 )		//扇区大小
#define STM32_FLASH_SIZE 	( 256 )			//FLASH大小
											
#define PACKET_SEQNO_INDEX      ( 1 )       //数据包序号
#define PACKET_SEQNO_COMP_INDEX ( 2 )       //包序取反

#define PACKET_HEADER           ( 3 )	  	//首部3位
#define PACKET_TRAILER          ( 2 )	  	//CRC检验的2位
#define PACKET_OVERHEAD         ( PACKET_HEADER + PACKET_TRAILER )//3位首部+2位CRC
#define PACKET_SIZE             ( 128 )     //128字节
#define PACKET_1K_SIZE          ( 1024 )    //1024字节

#define FILE_NAME_LENGTH        ( 256 )     //文件最大长度
#define FILE_SIZE_LENGTH        ( 16 )      //文件大小

#define SOH                     ( 0x01 )    //128字节数据包开始
#define STX                     ( 0x02 )    //1024字节的数据包开始
#define EOT                     ( 0x04 )    //结束传输
#define ACK                     ( 0x06 )    //回应
#define NAK                     ( 0x15 )    //没回应
#define CA                      ( 0x18 )    //这两个相继中止转移
#define CRC16                   ( 0x43 )    //'C'即0x43, 需要 16-bit CRC 

#define ABORT1                  ( 0x41 )    //'A'即0x41, 用户终止 
#define ABORT2                  ( 0x61 )    //'a'即0x61, 用户终止

#define NAK_TIMEOUT             ( 0x100000 )//最大超时时间
#define MAX_ERRORS              ( 5 )       //错误上限

#if defined (STM32F10X_MD) || defined (STM32F10X_MD_VL)
 #define PAGE_SIZE          (0x400)    //页的大小1K
 #define FLASH_SIZE         (0x20000)  //Flash空间128K
#elif defined STM32F10X_CL
 #define PAGE_SIZE          (0x800)    //页的大小2K
 #define FLASH_SIZE         (0x40000)  //Flash空间256K
#elif defined STM32F10X_HD || defined (STM32F10X_HD_VL)
 #define PAGE_SIZE          (0x800)    //页的大小2K
 #define FLASH_SIZE         (0x80000)  //Flash空间512K
#elif defined STM32F10X_XL
 #define PAGE_SIZE          (0x800)    //页的大小2K
 #define FLASH_SIZE         (0x100000) //Flash空间1M 
#else 
 #error "Please select first the STM32 device to be used (in stm32f10x.h)"    
#endif

#define IAP_PARAM_SIZE  1
#define IAP_PARAM_ADDR  (STM32_FLASH_BASE + FLASH_SIZE - PAGE_SIZE) //Flash空间最后1页地址开始处存放参数

void IAP_WaitForChoose(void);
void IAP_ShowMenu(void);
void iap_load_app(uint32_t appxaddr);

void cmd_boot(void);
void cmd_erase(void);
void cmd_update(void);

extern uint8_t flagIAP;

#endif
