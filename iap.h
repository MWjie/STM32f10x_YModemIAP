#ifndef __IAP_H__
#define __IAP_H__
#include <stdint.h>  

typedef  void (*iapfun)(void);				//����һ���������͵Ĳ���.

#define FLASH_APP1_ADDR		( 0x08018000 )  //��һ��Ӧ�ó�����ʼ��ַ(�����FLASH)
											
											
//FLASH��ʼ��ַ
#define STM32_FLASH_BASE 	( 0x08000000 ) 	//STM32 FLASH����ʼ��ַ
#define STM_SECTOR_SIZE		( 2048 )		//������С
#define STM32_FLASH_SIZE 	( 256 )			//FLASH��С
											
#define PACKET_SEQNO_INDEX      ( 1 )       //���ݰ����
#define PACKET_SEQNO_COMP_INDEX ( 2 )       //����ȡ��

#define PACKET_HEADER           ( 3 )	  	//�ײ�3λ
#define PACKET_TRAILER          ( 2 )	  	//CRC�����2λ
#define PACKET_OVERHEAD         ( PACKET_HEADER + PACKET_TRAILER )//3λ�ײ�+2λCRC
#define PACKET_SIZE             ( 128 )     //128�ֽ�
#define PACKET_1K_SIZE          ( 1024 )    //1024�ֽ�

#define FILE_NAME_LENGTH        ( 256 )     //�ļ���󳤶�
#define FILE_SIZE_LENGTH        ( 16 )      //�ļ���С

#define SOH                     ( 0x01 )    //128�ֽ����ݰ���ʼ
#define STX                     ( 0x02 )    //1024�ֽڵ����ݰ���ʼ
#define EOT                     ( 0x04 )    //��������
#define ACK                     ( 0x06 )    //��Ӧ
#define NAK                     ( 0x15 )    //û��Ӧ
#define CA                      ( 0x18 )    //�����������ֹת��
#define CRC16                   ( 0x43 )    //'C'��0x43, ��Ҫ 16-bit CRC 

#define ABORT1                  ( 0x41 )    //'A'��0x41, �û���ֹ 
#define ABORT2                  ( 0x61 )    //'a'��0x61, �û���ֹ

#define NAK_TIMEOUT             ( 0x100000 )//���ʱʱ��
#define MAX_ERRORS              ( 5 )       //��������

#if defined (STM32F10X_MD) || defined (STM32F10X_MD_VL)
 #define PAGE_SIZE          (0x400)    //ҳ�Ĵ�С1K
 #define FLASH_SIZE         (0x20000)  //Flash�ռ�128K
#elif defined STM32F10X_CL
 #define PAGE_SIZE          (0x800)    //ҳ�Ĵ�С2K
 #define FLASH_SIZE         (0x40000)  //Flash�ռ�256K
#elif defined STM32F10X_HD || defined (STM32F10X_HD_VL)
 #define PAGE_SIZE          (0x800)    //ҳ�Ĵ�С2K
 #define FLASH_SIZE         (0x80000)  //Flash�ռ�512K
#elif defined STM32F10X_XL
 #define PAGE_SIZE          (0x800)    //ҳ�Ĵ�С2K
 #define FLASH_SIZE         (0x100000) //Flash�ռ�1M 
#else 
 #error "Please select first the STM32 device to be used (in stm32f10x.h)"    
#endif

#define IAP_PARAM_SIZE  1
#define IAP_PARAM_ADDR  (STM32_FLASH_BASE + FLASH_SIZE - PAGE_SIZE) //Flash�ռ����1ҳ��ַ��ʼ����Ų���

void IAP_WaitForChoose(void);
void IAP_ShowMenu(void);
void iap_load_app(uint32_t appxaddr);

void cmd_boot(void);
void cmd_erase(void);
void cmd_update(void);

extern uint8_t flagIAP;

#endif
