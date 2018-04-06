#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "stm32f10x.h"
#include "stm32f10x_conf.h"
#include "iap.h"
#include "Shell.h"
#include "console.h"
#include "uart_all.h"

void YModem_Int2Str(uint8_t *str, int32_t intnum);
void iap_write_appbin(uint32_t appxaddr, uint8_t *appbuf, uint32_t appsize);
int8_t YModem_RecvPacket(uint8_t *data, int32_t *length, uint32_t timeout);
int32_t YModem_Receive(uint8_t *buf);
void iap_load_app(uint32_t appxaddr);
void IAP_SerialSendStr(uint8_t *str);
void IAP_SerialSendByte(uint8_t c);
void DownloadFirmware(void);
void UploadFirmware(void);
void IAP_DisableFlashWPR(void);
int8_t IAP_UpdataProgram(uint32_t addr, uint32_t size);
int8_t IAP_UpdataParam(int32_t *param);
void IAP_FlashEease(uint32_t size);

iapfun jump2app; 
uint8_t tab_1024[PACKET_1K_SIZE + PACKET_OVERHEAD];

const uint8_t *cmdFirmwareUpdata	= "FWUPDATA\r";  //Update the firmware to flash by YModem
const uint8_t *cmdFirmwareDownload	= "FWDWLOAD\r";  //Download the firmware from Flash by YModem
const uint8_t *cmdFirmwareErase 	= "FWERASE\r";   //Erase the current firmware
const uint8_t *cmdBoot 				= "BOOT\r"; 	 //Excute the current firmware
const uint8_t *cmdReboot 			= "REBOOT\r";    //Reboot
const uint8_t *cmdHelp			   	= "HELP\r";      //Display this help
const uint8_t *cmdQuit			   	= "Q\r";      	 //Quit bootloader

uint8_t file_name[64], file_size[16];

uint8_t flagIAP = 0;

void IAP_ShowMenu(void)
{
	IAP_SerialSendStr("\r\n+================(C) COPYRIGHT **** ******* ================+");
	IAP_SerialSendStr("\r\n|    In-Application Programing Application (Version 1.0)    |");
	IAP_SerialSendStr("\r\n+----command----+-----------------function------------------+");
	IAP_SerialSendStr("\r\n|    FWUPDATA   | Update the firmware to flash by YModem    |");
	IAP_SerialSendStr("\r\n|    FWDWLOAD   | Download the firmware from Flash by YModem|");
	IAP_SerialSendStr("\r\n|    FWERASE    | Erase the current firmware                |");
	IAP_SerialSendStr("\r\n|    BOOT       | Excute the current firmware               |");
	IAP_SerialSendStr("\r\n|    REBOOT     | Reboot                                    |");
	IAP_SerialSendStr("\r\n|    HELP       | Display this help                         |");
	IAP_SerialSendStr("\r\n|    Q          | Quit bootloader                           |");
	IAP_SerialSendStr("\r\n+===========================================================+");
	IAP_SerialSendStr("\r\n\r\n");
	IAP_SerialSendStr("STM32-IAP>>");
}

void cmd_update(void)
{
	file_len = 0;
	flagIAP = 1;
	IAP_DisableFlashWPR();
	IAP_FlashEease(FLASH_SIZE + STM32_FLASH_BASE - FLASH_APP1_ADDR);//����Flash�������������flash�ռ�
	DownloadFirmware();//��ʼ����
	Delay_ms(500);
	flagIAP = 0;
//	iap_load_app(FLASH_APP1_ADDR);
//	NVIC_SystemReset();//����
	memset(console_rx_buff, 0, sizeof(console_rx_buff));
}

void cmd_erase(void)
{
	IAP_SerialSendStr("\r\nErasing...");
	IAP_FlashEease(FLASH_SIZE + STM32_FLASH_BASE - FLASH_APP1_ADDR);//����Flash�������������flash�ռ�
	IAP_SerialSendStr("\r\nErase done!\r\n");
	memset(console_rx_buff, 0, sizeof(console_rx_buff));
}

void cmd_boot(void)
{
	IAP_SerialSendStr("\r\nBotting...\r\n");
	if (((*(__IO uint32_t *)FLASH_APP1_ADDR) & 0x2FFE0000) != 0x20000000) {//�ж��Ƿ���Ӧ�ó���
		IAP_SerialSendStr("No user program! Please download a firmware!\r\n");
		return;
	}
	iap_load_app(FLASH_APP1_ADDR);
	Delay_ms(500);
	NVIC_SystemReset();
	memset(console_rx_buff, 0, sizeof(console_rx_buff));
}

void IAP_WaitForChoose(void)
{
	while (1) {
		if (!strcmp((const char*)cmdFirmwareDownload, (const char*)console_rx_buff)) {
			cmd_update();
		}
		if (!strcmp((const char*)cmdFirmwareUpdata, (const char*)console_rx_buff)) {
	//		UploadFirmware();//��ʼ�ϴ�
			memset(console_rx_buff, 0, sizeof(console_rx_buff));
		}
		if (!strcmp((const char*)cmdFirmwareErase, (const char*)console_rx_buff)) {//FWERASE�̼�����
			cmd_erase();
		}
		if (!strcmp((const char*)cmdBoot, (const char*)console_rx_buff)) {//BOOTִ����������
			cmd_boot();
		}
		if (!strcmp((const char*)cmdReboot, (const char*)console_rx_buff)) {//REBOOTϵͳ����
			IAP_SerialSendStr("\r\nRebooting...\r\n");
			NVIC_SystemReset();
			memset(console_rx_buff, 0, sizeof(console_rx_buff));
		}
		if (!strcmp((const char*)cmdHelp, (const char*)console_rx_buff)) {//HELP����
	//		ShwHelpInfo();//��ʾ������Ϣ
			memset(console_rx_buff, 0, sizeof(console_rx_buff));
		}
		if (!strcmp((const char*)cmdQuit, (const char*)console_rx_buff)) {//�˳�Bootloader
			break;
		}
		if (sysTime.RunFlag) {
			Fet_Dog();
			sysTime.RunFlag = 0;
		}
		if (sysTime.SendFlag) {
			//NetSendTest();
			sysTime.SendFlag = 0;
		}
		if (Mn_Shell_States == Get_New_Comd) {
			shell_work();
		} 
	}
}


void DownloadFirmware(void)
{
	uint8_t number[10]= "          ";        //�ļ��Ĵ�С�ַ���
	int32_t size = 0;

	IAP_SerialSendStr("\r\nWaiting for the file to be send...(press 'a' or 'A' to abort)\r\n");
	size = YModem_Receive(&tab_1024[0]);//��ʼ������������
	Delay_ms(1000);//��ʱ1s����secureCRT���㹻ʱ��ر�ymodem�Ի�������Ӱ���������Ϣ��ӡ
	if (size > 0) {
		IAP_SerialSendStr("+-----------------------------------+\r\n");
		IAP_SerialSendStr("Proramming completed successfully!\r\nName: ");
		IAP_SerialSendStr(file_name);//��ʾ�ļ���
		YModem_Int2Str(number, size);
		IAP_SerialSendStr("\r\nSize:");
		IAP_SerialSendStr(number);//��ʾ�ļ���С
		IAP_SerialSendStr("Bytes\r\n"); 
		IAP_SerialSendStr("+-----------------------------------+\r\n");	
	}
	else if(size == -1) {//�̼��Ĵ�С������������flash�ռ�
		IAP_SerialSendStr("The image size is higher than the allowed space memory!\r\n");
	}
	else if(size == -2) {//������д���ɹ�
		IAP_SerialSendStr("Verification failed!\r\n");
	}
	else if(size == -3) {//�û���ֹ
		IAP_SerialSendStr("Aborted by user!\r\n");
	}
	else {//��������
		IAP_SerialSendStr("Failed to receive the file!\r\n");	
	}
}

int32_t YModem_Receive(uint8_t *buf)
{
	uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD], file_size[FILE_SIZE_LENGTH];	
	uint8_t session_done, file_done, session_begin, packets_received, errors; 	
	uint8_t *file_ptr, *buf_ptr;
	int32_t packet_length = 0, size = 0;
	int32_t i = 0, RamSource = 0;

	for (session_done = 0, errors = 0, session_begin = 0; ;) {//��ѭ����һ��ymodem����	
		for (packets_received = 0, file_done = 0, buf_ptr = buf; ; ) {//��ѭ�������Ͻ�������
			switch (YModem_RecvPacket(packet_data, &packet_length, NAK_TIMEOUT)) {//�������ݰ�		
				case 0:
					errors = 0;
					switch (packet_length) {
						case -1: //���Ͷ���ֹ����
							IAP_SerialSendByte(ACK);//�ظ�ACK
							return 0;
						case 0:	//���ս�������մ���
							file_done = 1;//�������
							session_done = 1;//�����Ի�
							break;
						default: //����������
							if ((packet_data[PACKET_SEQNO_INDEX] & 0xff) != (packets_received & 0xff)) {
								IAP_SerialSendByte(NAK);//���մ�������ݣ��ظ�NAK
							} else {//���յ���ȷ������					
								if (packets_received == 0) {//���յ�һ֡����
									if (packet_data[PACKET_HEADER] != 0) {//�����ļ���Ϣ���ļ������ļ����ȵ�							
										for (i = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && (i < FILE_NAME_LENGTH); ) {		
											file_name[i++] = *file_ptr++;//�����ļ���
										}
										file_name[i++] = '\0';//�ļ�����'\0'����

										for (i = 0, file_ptr++; (*file_ptr != ' ') && (i < FILE_SIZE_LENGTH); ) {
											file_size[i++] = *file_ptr++;//�����ļ���С
										}
										file_size[i++] = '\0';//�ļ���С��'\0'����
										size = atoi((const char *)file_size);//���ļ���С�ַ���ת�������� 

										if (size > (FLASH_SIZE -1)) {//�����̼�����
											IAP_SerialSendByte(CA);			 
											IAP_SerialSendByte(CA);//��������2����ֹ��CA
											return -1;//����
										}
										IAP_FlashEease(size);//������Ӧ��flash�ռ�
										IAP_UpdataParam(&size);//��size��С��д��Flash��Parameter��
										IAP_SerialSendByte(ACK);//�ظ�ACk
										IAP_SerialSendByte(CRC16);//����'C',ѯ������
									}
									else {//�ļ������ݰ�Ϊ�գ���������
										IAP_SerialSendByte(ACK);//�ظ�ACK
										file_done = 1;//ֹͣ����
										session_done = 1;//�����Ի�
										break;
									}
								}
								else {//�յ����ݰ�
									memcpy(buf_ptr, packet_data + PACKET_HEADER, packet_length);//��������
									RamSource = (uint32_t)buf;//8λǿ��ת����32Ϊ����
									
									if (IAP_UpdataProgram(RamSource, packet_length) != 0) {       //��д��������	
										IAP_SerialSendByte(CA);	 
										IAP_SerialSendByte(CA);//flash��д������������2����ֹ��CA
										return -2;//��д����
									}
									file_len = 0;
									IAP_SerialSendByte(ACK);//flash��д�ɹ����ظ�ACK
								}
								packets_received++;//�յ����ݰ��ĸ���
								session_begin = 1;//���ý����б�־
							}
						}
						break;
				case 1:		                //�û���ֹ
					IAP_SerialSendByte(CA);
					IAP_SerialSendByte(CA);    //��������2����ֹ��CA
					return -3;		//��д��ֹ
				default:
				if (session_begin > 0) {  //��������з�������	
					errors++;
				}
				if (errors > MAX_ERRORS) {//���󳬹�����
					IAP_SerialSendByte(CA);
					IAP_SerialSendByte(CA);//��������2����ֹ��CA
					return 0;	//������̷����������
				}
				IAP_SerialSendByte(CRC16); //����'C',��������
				break;
			}  
			if (file_done != 0) {//�ļ�������ϣ��˳�ѭ��
				IAP_SerialSendByte(ACK);
				break;
			}
		}
		if (session_done != 0) {//�Ի�����������ѭ��
			IAP_SerialSendByte(ACK);//�ظ�ACk
			break;
		}
	}
	return (int32_t)size;//���ؽ��յ��ļ��Ĵ�С
}

int8_t YModem_RecvPacket(uint8_t *data, int32_t *length, uint32_t timeout)
{
	uint16_t packet_size;
	uint32_t i;
	uint32_t count_time;
	*length = 0;
	

	switch (file_buf[0]) {
		case SOH:				//128�ֽ����ݰ�
			packet_size = PACKET_SIZE;	//��¼���ݰ��ĳ���
			break;
		case STX:				//1024�ֽ����ݰ�
			packet_size = PACKET_1K_SIZE;	//��¼���ݰ��ĳ���
			break;
		case EOT:				//���ݽ��ս����ַ�
			return 0;           //���ս���			 
		case CA:				//������ֹ��־
			if (file_buf[1] == CA) {	//�ȴ�������ֹ�ַ�
				*length = -1;			//���յ���ֹ�ַ�
				return 0;					 
			} else { //���ճ�ʱ
				return -1;
			}
		case ABORT1:				//�û���ֹ���û�����'A'
		case ABORT2:				//�û���ֹ���û�����'a'
			return 1;                       //������ֹ
		default:
			Delay_ms(500);
			return -1;                      //���մ���
	}
	count_time = sysTime.sTime;
	while (1) {
		if (file_buf[0] == EOT) {
			IAP_SerialSendByte(ACK);
			file_len = 0;
			return 0;
		}
		if (file_len >= packet_size + PACKET_OVERHEAD) {
			file_len = 0;
			break;
		}
		if (sysTime.sTime - count_time > 2) {
			memset(file_buf+file_len, 0, sizeof(file_buf)-file_len);
			file_len = 0;
			break;
		}
	}
	for (i = 0; i < (packet_size + PACKET_OVERHEAD); i++) {//��������
		data[i] = file_buf[i];
	}

	if (data[PACKET_SEQNO_INDEX] != ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff)) {
		return -1;                   //���մ���	
	}
//	memcpy(data, file_buf, packet_size + PACKET_OVERHEAD);

	*length = packet_size;               //������յ������ݳ���
	file_len = 0;
	return 0;                            //��������
}

//IAP �����ַ���
void IAP_SerialSendStr(uint8_t *str)
{
	while (*str != '\0') {
		IAP_SerialSendByte(*str);
		str++;
	}
}

//IAP ����һ���ֽ�
void IAP_SerialSendByte(uint8_t c)
{
	USART_SendData(USART1, c);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
		;
	}
}



void YModem_Int2Str(uint8_t *str, int32_t intnum)
{
    uint32_t i, Div = 1000000000, j = 0, Status = 0;

    for (i = 0; i < 10; i++) {
        str[j++] = (intnum / Div) + '0';//����ת�����ַ�
        intnum = intnum % Div;				
        Div /= 10;
        if ((str[j-1] == '0') & (Status == 0)) {//������ǰ���'0'
            j = 0;
        } else {
            Status++;
        }
    }
}



void IAP_DisableFlashWPR(void)
{
	uint32_t blockNum = 0, UserMemoryMask = 0;

    blockNum = (FLASH_APP1_ADDR - STM32_FLASH_BASE) >> 12;   //����flash��
	UserMemoryMask = ((uint32_t)(~((1 << blockNum) - 1)));//��������

	if ((FLASH_GetWriteProtectionOptionByte() & UserMemoryMask) != UserMemoryMask) {//�鿴�����������Ƿ�д����
		FLASH_EraseOptionBytes();//����ѡ��λ
	}
}

int8_t IAP_UpdataParam(int32_t *param)
{
	uint32_t i;
	uint32_t flashptr = IAP_PARAM_ADDR;

	FLASH_Unlock();//flash����
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);//���flash��ر�־λ
	for (i = 0; i < IAP_PARAM_SIZE; i++) {
		FLASH_ProgramWord(flashptr + 4 * i, *param);
		if (*(uint32_t *)(flashptr + 4 * i) != *param) {
			return -1;
		}	
		param++;
	}
	FLASH_Lock();//flash����
	return 0;
}

int8_t IAP_UpdataProgram(uint32_t addr, uint32_t size)
{
	uint32_t i;
	static uint32_t flashptr = FLASH_APP1_ADDR;
	
	FLASH_Unlock();//flash����
	FLASH_ErasePage(IAP_PARAM_ADDR);//�����������ڵ�flashҳ
	FLASH_SetLatency(FLASH_Latency_2);//72MHzϵͳʱ������������ʱ����ʱ
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);//���flash��ر�־λ
	for (i = 0; i < size; i += 4) {
		FLASH_ProgramWord(flashptr, *(uint32_t *)addr);//��д1����
		if (*(uint32_t *)flashptr != *(uint32_t *)addr) {//�ж��Ƿ���д�ɹ�
			return -1;
		}
		flashptr += 4;
		addr += 4;
	}
	FLASH_Lock();//flash����
	return 0;
}

void IAP_FlashEease(uint32_t size)
{
	uint16_t eraseCounter = 0;
	uint16_t nbrOfPage = 0;
	FLASH_Status FLASHStatus = FLASH_COMPLETE;	  

	if (size % PAGE_SIZE != 0) {//������Ҫ��д��ҳ��
		nbrOfPage = size / PAGE_SIZE + 1; 
	} else {
		nbrOfPage = size / PAGE_SIZE;
	}

	FLASH_Unlock();//���flash��д����
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);//���flash��ر�־λ
	for (eraseCounter = 0; (eraseCounter < nbrOfPage) && ((FLASHStatus == FLASH_COMPLETE)); eraseCounter++) {//��ʼ����
		FLASHStatus = FLASH_ErasePage(FLASH_APP1_ADDR + (eraseCounter * PAGE_SIZE));//����
		IAP_SerialSendStr(".");//��ӡ'.'����ʾ����
	}
	FLASH_ErasePage(IAP_PARAM_ADDR);//�����������ڵ�flashҳ
	FLASH_Lock();//flash��д����
}

//����ջ����ַ
//addr:ջ����ַ
__asm void MSR_MSP(uint32_t addr)
{
    MSR MSP, r0 			//set Main Stack value
    BX r14
}

//��ת��Ӧ�ó����
//appxaddr:�û�������ʼ��ַ.
void iap_load_app(uint32_t appxaddr)
{
	USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
	USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
	TIM_Cmd(TIM5, DISABLE);
	NVIC_DisableIRQ(EXTI1_IRQn);
	SysTick->CTRL = 0x00000008;
	__disable_fault_irq();
	
	if (((*(volatile uint32_t*)appxaddr)&0x2FFE0000) == 0x20000000) {	//���ջ����ַ�Ƿ�Ϸ�.
		jump2app = (iapfun)*(volatile uint32_t*)(appxaddr+4);			//�û��������ڶ�����Ϊ����ʼ��ַ(��λ��ַ)		
		MSR_MSP(*(volatile uint32_t*)appxaddr);							//��ʼ��APP��ջָ��(�û��������ĵ�һ�������ڴ��ջ����ַ)
		jump2app();														//��ת��APP.
	}
}		 
