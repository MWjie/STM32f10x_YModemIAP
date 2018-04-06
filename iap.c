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
	IAP_FlashEease(FLASH_SIZE + STM32_FLASH_BASE - FLASH_APP1_ADDR);//擦除Flash，升级处后面的flash空间
	DownloadFirmware();//开始升级
	Delay_ms(500);
	flagIAP = 0;
//	iap_load_app(FLASH_APP1_ADDR);
//	NVIC_SystemReset();//重启
	memset(console_rx_buff, 0, sizeof(console_rx_buff));
}

void cmd_erase(void)
{
	IAP_SerialSendStr("\r\nErasing...");
	IAP_FlashEease(FLASH_SIZE + STM32_FLASH_BASE - FLASH_APP1_ADDR);//擦除Flash，升级处后面的flash空间
	IAP_SerialSendStr("\r\nErase done!\r\n");
	memset(console_rx_buff, 0, sizeof(console_rx_buff));
}

void cmd_boot(void)
{
	IAP_SerialSendStr("\r\nBotting...\r\n");
	if (((*(__IO uint32_t *)FLASH_APP1_ADDR) & 0x2FFE0000) != 0x20000000) {//判断是否有应用程序
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
	//		UploadFirmware();//开始上传
			memset(console_rx_buff, 0, sizeof(console_rx_buff));
		}
		if (!strcmp((const char*)cmdFirmwareErase, (const char*)console_rx_buff)) {//FWERASE固件擦除
			cmd_erase();
		}
		if (!strcmp((const char*)cmdBoot, (const char*)console_rx_buff)) {//BOOT执行升级程序
			cmd_boot();
		}
		if (!strcmp((const char*)cmdReboot, (const char*)console_rx_buff)) {//REBOOT系统重启
			IAP_SerialSendStr("\r\nRebooting...\r\n");
			NVIC_SystemReset();
			memset(console_rx_buff, 0, sizeof(console_rx_buff));
		}
		if (!strcmp((const char*)cmdHelp, (const char*)console_rx_buff)) {//HELP帮助
	//		ShwHelpInfo();//显示帮助信息
			memset(console_rx_buff, 0, sizeof(console_rx_buff));
		}
		if (!strcmp((const char*)cmdQuit, (const char*)console_rx_buff)) {//退出Bootloader
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
	uint8_t number[10]= "          ";        //文件的大小字符串
	int32_t size = 0;

	IAP_SerialSendStr("\r\nWaiting for the file to be send...(press 'a' or 'A' to abort)\r\n");
	size = YModem_Receive(&tab_1024[0]);//开始接收升级程序
	Delay_ms(1000);//延时1s，让secureCRT有足够时间关闭ymodem对话，而不影响下面的信息打印
	if (size > 0) {
		IAP_SerialSendStr("+-----------------------------------+\r\n");
		IAP_SerialSendStr("Proramming completed successfully!\r\nName: ");
		IAP_SerialSendStr(file_name);//显示文件名
		YModem_Int2Str(number, size);
		IAP_SerialSendStr("\r\nSize:");
		IAP_SerialSendStr(number);//显示文件大小
		IAP_SerialSendStr("Bytes\r\n"); 
		IAP_SerialSendStr("+-----------------------------------+\r\n");	
	}
	else if(size == -1) {//固件的大小超出处理器的flash空间
		IAP_SerialSendStr("The image size is higher than the allowed space memory!\r\n");
	}
	else if(size == -2) {//程序烧写不成功
		IAP_SerialSendStr("Verification failed!\r\n");
	}
	else if(size == -3) {//用户终止
		IAP_SerialSendStr("Aborted by user!\r\n");
	}
	else {//其他错误
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

	for (session_done = 0, errors = 0, session_begin = 0; ;) {//死循环，一个ymodem连接	
		for (packets_received = 0, file_done = 0, buf_ptr = buf; ; ) {//死循环，不断接收数据
			switch (YModem_RecvPacket(packet_data, &packet_length, NAK_TIMEOUT)) {//接收数据包		
				case 0:
					errors = 0;
					switch (packet_length) {
						case -1: //发送端中止传输
							IAP_SerialSendByte(ACK);//回复ACK
							return 0;
						case 0:	//接收结束或接收错误
							file_done = 1;//接收完成
							session_done = 1;//结束对话
							break;
						default: //接收数据中
							if ((packet_data[PACKET_SEQNO_INDEX] & 0xff) != (packets_received & 0xff)) {
								IAP_SerialSendByte(NAK);//接收错误的数据，回复NAK
							} else {//接收到正确的数据					
								if (packets_received == 0) {//接收第一帧数据
									if (packet_data[PACKET_HEADER] != 0) {//包含文件信息：文件名，文件长度等							
										for (i = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && (i < FILE_NAME_LENGTH); ) {		
											file_name[i++] = *file_ptr++;//保存文件名
										}
										file_name[i++] = '\0';//文件名以'\0'结束

										for (i = 0, file_ptr++; (*file_ptr != ' ') && (i < FILE_SIZE_LENGTH); ) {
											file_size[i++] = *file_ptr++;//保存文件大小
										}
										file_size[i++] = '\0';//文件大小以'\0'结束
										size = atoi((const char *)file_size);//将文件大小字符串转换成整型 

										if (size > (FLASH_SIZE -1)) {//升级固件过大
											IAP_SerialSendByte(CA);			 
											IAP_SerialSendByte(CA);//连续发送2次中止符CA
											return -1;//返回
										}
										IAP_FlashEease(size);//擦除相应的flash空间
										IAP_UpdataParam(&size);//将size大小烧写进Flash中Parameter区
										IAP_SerialSendByte(ACK);//回复ACk
										IAP_SerialSendByte(CRC16);//发送'C',询问数据
									}
									else {//文件名数据包为空，结束传输
										IAP_SerialSendByte(ACK);//回复ACK
										file_done = 1;//停止接收
										session_done = 1;//结束对话
										break;
									}
								}
								else {//收到数据包
									memcpy(buf_ptr, packet_data + PACKET_HEADER, packet_length);//拷贝数据
									RamSource = (uint32_t)buf;//8位强制转化成32为数据
									
									if (IAP_UpdataProgram(RamSource, packet_length) != 0) {       //烧写升级数据	
										IAP_SerialSendByte(CA);	 
										IAP_SerialSendByte(CA);//flash烧写错误，连续发送2次中止符CA
										return -2;//烧写错误
									}
									file_len = 0;
									IAP_SerialSendByte(ACK);//flash烧写成功，回复ACK
								}
								packets_received++;//收到数据包的个数
								session_begin = 1;//设置接收中标志
							}
						}
						break;
				case 1:		                //用户中止
					IAP_SerialSendByte(CA);
					IAP_SerialSendByte(CA);    //连续发送2次中止符CA
					return -3;		//烧写中止
				default:
				if (session_begin > 0) {  //传输过程中发生错误	
					errors++;
				}
				if (errors > MAX_ERRORS) {//错误超过上限
					IAP_SerialSendByte(CA);
					IAP_SerialSendByte(CA);//连续发送2次中止符CA
					return 0;	//传输过程发生过多错误
				}
				IAP_SerialSendByte(CRC16); //发送'C',继续接收
				break;
			}  
			if (file_done != 0) {//文件接收完毕，退出循环
				IAP_SerialSendByte(ACK);
				break;
			}
		}
		if (session_done != 0) {//对话结束，跳出循环
			IAP_SerialSendByte(ACK);//回复ACk
			break;
		}
	}
	return (int32_t)size;//返回接收到文件的大小
}

int8_t YModem_RecvPacket(uint8_t *data, int32_t *length, uint32_t timeout)
{
	uint16_t packet_size;
	uint32_t i;
	uint32_t count_time;
	*length = 0;
	

	switch (file_buf[0]) {
		case SOH:				//128字节数据包
			packet_size = PACKET_SIZE;	//记录数据包的长度
			break;
		case STX:				//1024字节数据包
			packet_size = PACKET_1K_SIZE;	//记录数据包的长度
			break;
		case EOT:				//数据接收结束字符
			return 0;           //接收结束			 
		case CA:				//接收中止标志
			if (file_buf[1] == CA) {	//等待接收中止字符
				*length = -1;			//接收到中止字符
				return 0;					 
			} else { //接收超时
				return -1;
			}
		case ABORT1:				//用户终止，用户按下'A'
		case ABORT2:				//用户终止，用户按下'a'
			return 1;                       //接收终止
		default:
			Delay_ms(500);
			return -1;                      //接收错误
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
	for (i = 0; i < (packet_size + PACKET_OVERHEAD); i++) {//接收数据
		data[i] = file_buf[i];
	}

	if (data[PACKET_SEQNO_INDEX] != ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff)) {
		return -1;                   //接收错误	
	}
//	memcpy(data, file_buf, packet_size + PACKET_OVERHEAD);

	*length = packet_size;               //保存接收到的数据长度
	file_len = 0;
	return 0;                            //正常接收
}

//IAP 发送字符串
void IAP_SerialSendStr(uint8_t *str)
{
	while (*str != '\0') {
		IAP_SerialSendByte(*str);
		str++;
	}
}

//IAP 发送一个字节
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
        str[j++] = (intnum / Div) + '0';//数字转化成字符
        intnum = intnum % Div;				
        Div /= 10;
        if ((str[j-1] == '0') & (Status == 0)) {//忽略最前面的'0'
            j = 0;
        } else {
            Status++;
        }
    }
}



void IAP_DisableFlashWPR(void)
{
	uint32_t blockNum = 0, UserMemoryMask = 0;

    blockNum = (FLASH_APP1_ADDR - STM32_FLASH_BASE) >> 12;   //计算flash块
	UserMemoryMask = ((uint32_t)(~((1 << blockNum) - 1)));//计算掩码

	if ((FLASH_GetWriteProtectionOptionByte() & UserMemoryMask) != UserMemoryMask) {//查看块所在区域是否写保护
		FLASH_EraseOptionBytes();//擦除选择位
	}
}

int8_t IAP_UpdataParam(int32_t *param)
{
	uint32_t i;
	uint32_t flashptr = IAP_PARAM_ADDR;

	FLASH_Unlock();//flash上锁
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);//清除flash相关标志位
	for (i = 0; i < IAP_PARAM_SIZE; i++) {
		FLASH_ProgramWord(flashptr + 4 * i, *param);
		if (*(uint32_t *)(flashptr + 4 * i) != *param) {
			return -1;
		}	
		param++;
	}
	FLASH_Lock();//flash解锁
	return 0;
}

int8_t IAP_UpdataProgram(uint32_t addr, uint32_t size)
{
	uint32_t i;
	static uint32_t flashptr = FLASH_APP1_ADDR;
	
	FLASH_Unlock();//flash上锁
	FLASH_ErasePage(IAP_PARAM_ADDR);//擦除参数所在的flash页
	FLASH_SetLatency(FLASH_Latency_2);//72MHz系统时钟下设置两个时钟延时
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);//清除flash相关标志位
	for (i = 0; i < size; i += 4) {
		FLASH_ProgramWord(flashptr, *(uint32_t *)addr);//烧写1个字
		if (*(uint32_t *)flashptr != *(uint32_t *)addr) {//判断是否烧写成功
			return -1;
		}
		flashptr += 4;
		addr += 4;
	}
	FLASH_Lock();//flash解锁
	return 0;
}

void IAP_FlashEease(uint32_t size)
{
	uint16_t eraseCounter = 0;
	uint16_t nbrOfPage = 0;
	FLASH_Status FLASHStatus = FLASH_COMPLETE;	  

	if (size % PAGE_SIZE != 0) {//计算需要擦写的页数
		nbrOfPage = size / PAGE_SIZE + 1; 
	} else {
		nbrOfPage = size / PAGE_SIZE;
	}

	FLASH_Unlock();//解除flash擦写锁定
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);//清除flash相关标志位
	for (eraseCounter = 0; (eraseCounter < nbrOfPage) && ((FLASHStatus == FLASH_COMPLETE)); eraseCounter++) {//开始擦除
		FLASHStatus = FLASH_ErasePage(FLASH_APP1_ADDR + (eraseCounter * PAGE_SIZE));//擦除
		IAP_SerialSendStr(".");//打印'.'以显示进度
	}
	FLASH_ErasePage(IAP_PARAM_ADDR);//擦除参数所在的flash页
	FLASH_Lock();//flash擦写锁定
}

//设置栈顶地址
//addr:栈顶地址
__asm void MSR_MSP(uint32_t addr)
{
    MSR MSP, r0 			//set Main Stack value
    BX r14
}

//跳转到应用程序段
//appxaddr:用户代码起始地址.
void iap_load_app(uint32_t appxaddr)
{
	USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
	USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
	TIM_Cmd(TIM5, DISABLE);
	NVIC_DisableIRQ(EXTI1_IRQn);
	SysTick->CTRL = 0x00000008;
	__disable_fault_irq();
	
	if (((*(volatile uint32_t*)appxaddr)&0x2FFE0000) == 0x20000000) {	//检查栈顶地址是否合法.
		jump2app = (iapfun)*(volatile uint32_t*)(appxaddr+4);			//用户代码区第二个字为程序开始地址(复位地址)		
		MSR_MSP(*(volatile uint32_t*)appxaddr);							//初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址)
		jump2app();														//跳转到APP.
	}
}		 
