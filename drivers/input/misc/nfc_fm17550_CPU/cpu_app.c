#include <linux/slab.h>
#include <linux/string.h>
#include "cpu_card.h"
#include "des.h"
#include "type_a.h"
#include "spi.h"
#include "fm175xx.h"
#include "../mg_netlink.h"

#define APDU_Debug 0

unsigned char CPU_Send_Buff[255],CPU_Rece_Buff[255],CPU_Send_Len,CPU_Rece_Len;

unsigned char APDU_Exchange(unsigned char send_len,unsigned char *send_buff,unsigned char *rece_len,unsigned char *rece_buff)
{
	unsigned char result,tpdu_rx_len;
	int i = 0,j = 0;
	*rece_len=0;

	#if APDU_Debug
	pr_info("nfc_fm17750<- CPU_CARD.PCB = 0x%02X",CPU_CARD.PCB);
	pr_info("nfc_fm17750<- CPU_CARD.CID = 0x%02X",CPU_CARD.CID);
	pr_info("nfc_fm17750<- CPU_CARD.FSD = 0x%02X",CPU_CARD.FSD);

	#endif 

	if(send_len>CPU_CARD.FSD)//发送长度不能超过卡片ATS中定义的最大接收帧长度
	{
		pr_info("nfc_fm17750<- APDU Send Length Overflow\n");
		return ERROR;
	}
	
	result=	CPU_I_Block(send_len,send_buff,&(*rece_len),rece_buff);
	
	#if APDU_Debug
	pr_info("nfc_fm17750<- result = 0x%02X\n",result);
	pr_info("nfc_fm17750<- rece_buff = ");
	for(i = 0;i < *rece_len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",rece_buff[j]);
	}
	#endif

	if (result==OK)
	{
		while(1)
		{
			j ++;
			if(j > 10)
				break;
			if(( CPU_CARD.PCB&0xF0)==0xF0)//WTX
			{
				result=CPU_WTX(CPU_CARD.WTXM,&tpdu_rx_len,rece_buff);
				if(result!=OK)
					return ERROR;
				CPU_CARD.PCB=*rece_buff;
				if(( CPU_CARD.PCB&0xF0)==0xF0)//WTX
				{
				if( CPU_CARD.PCB&0x08)
					CPU_CARD.WTXM=*(rece_buff+2);
				else
					CPU_CARD.WTXM=*(rece_buff+1);
				}
				*rece_len=tpdu_rx_len;
			}
			
			if((CPU_CARD.PCB&0xF0)==0x10)//R_Block
			{
				CPU_PCB_CONVER();
				result=CPU_R_Block(&tpdu_rx_len,rece_buff+(*rece_len));
				if(result!=OK)
					return ERROR;
				CPU_CARD.PCB=*(rece_buff+(*rece_len));
				if( CPU_CARD.PCB&0x08)
				{
					for (i=0;i<tpdu_rx_len-2;i++)
						*(rece_buff+i+(*rece_len))=*(rece_buff+i+(*rece_len)+2);
					*rece_len=*rece_len+tpdu_rx_len-2;
				}
				else
				{
					for (i=0;i<tpdu_rx_len-1;i++)
						*(rece_buff+i+(*rece_len))=*(rece_buff+i+(*rece_len)+1);
					*rece_len=*rece_len+tpdu_rx_len-1;
				}
			}

			if(( CPU_CARD.PCB&0xF0)==0x00)//I_Block
			{
				if(result==OK)
				{
					CPU_PCB_CONVER();
					if( CPU_CARD.PCB&0x08)//cid存在
					{
						*rece_len=*rece_len-2;
						memmove(rece_buff,rece_buff+2,*rece_len);
					}
					else
					{
						*rece_len=*rece_len-1;
						memmove(rece_buff,rece_buff+1,*rece_len);
					}
					return OK;
				}
				else
					return ERROR;
			}
		}
	}
	return ERROR;
}

int call_apdu_cmd(unsigned char *msg,int msglen,unsigned char * msg_out)
{
	unsigned char result;
	unsigned char rtn;
	
	
	pr_info("%d/%s :entry,msglen:%d\n",__LINE__,__FUNCTION__,msglen);
//	FM175XX_Initial_ReaderA();
//	Set_Rf(3);
//	msleep(200);
/*	
	if(TypeA_CardActivate(PICC_ATQA,PICC_UID,PICC_SAK) != OK)
	{
		pr_info("call_apdu_cmd-> TypeA_CardActivate ERROR\n");
		CPU_Rece_Len = 0;
		goto EXIT_OK;
	}
	result = CPU_Rats(0x02,0x50,&CPU_Rece_Len,CPU_Rece_Buff);//CPU卡片RATS操作 14443-4xieyi                                                                                                                               
	if(result != OK)
	{
		pr_info("call_apdu_cmd-> Card RATS ERROR\n");
		CPU_Rece_Len = 0;
		goto EXIT_OK;
	}
*/	
	result = APDU_Exchange(msglen,msg,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result != OK)
	{
		pr_info("call_apdu_cmd-> Select MF ERROR\n");
		CPU_Rece_Len = 0;
		goto EXIT_OK;
	}
	
	memcpy(msg_out,CPU_Rece_Buff,CPU_Rece_Len);
	
EXIT_OK:
//	Set_Rf(0);	
	
	return CPU_Rece_Len;
}

int call_rats(void)
{
	if (CPU_Rats(0x02,0x50,&CPU_Rece_Len,CPU_Rece_Buff) == OK){
		pr_info("call_rats-> Card RATS SUCCESS\n");
		return 1;
	} else {
		pr_info("call_rats-> Card RATS ERROR\n");
		return 0;
	}
}

unsigned char CPU_APP(unsigned char *msg)
{
	unsigned char result;
	int i = 0;
	unsigned char *cmd_ack = NULL;
	unsigned char rats_error[]={0x00,0x00,0x05,0x00,0x01};

	result = CPU_Rats(0x02,0x50,&CPU_Rece_Len,CPU_Rece_Buff);//CPU卡片RATS操作 14443-4xieyi                                                                                                                                                                                                                                                                 
	if(result != OK)
	{
		pr_info("nfc_fm17750-> Card RATS ERROR\n");
		send_mg_msg(rats_error,sizeof(rats_error));
		return ERROR;
	}
	#if APDU_Debug
	pr_info("nfc_fm17750-> Card RATS OK\n");
	pr_info("nfc_fm17750<- Ats = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}
	#endif
	// result = Tdes( unsigned char Mode,unsigned char *MsgIn, unsigned char *Key, unsigned char *MsgOut);
#if 0	
	CPU_Send_Len=5;
  	memcpy(CPU_Send_Buff,"\x00\x84\x00\x00\x08",CPU_Send_Len);//选择MF文件
  	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select MF ERROR\n");
		return ERROR;
	}
	#if APDU_Debug
	pr_info("nfc_fm17750-> 84 OK\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}
	#endif
#endif	
#if 1
	/////////////////////////////handle netlink cmd/////////////////////////////////////
	CPU_Send_Len = msg[1] - 4;
	memcpy(CPU_Send_Buff,(msg+4),CPU_Send_Len);
	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> handle netlink cmd ERROR\n");
		return ERROR;
	}
	else
	{
		#if APDU_Debug

		pr_info("nfc_fm17750 len = 0x%02X\n",CPU_Rece_Len);
		for(i = 0;i < CPU_Rece_Len;i ++)
		{
			pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
		}
		#endif
		cmd_ack = kmalloc(CPU_Rece_Len + 5,GFP_KERNEL);
		cmd_ack[0] = CMD_NFC;
		cmd_ack[1] = 0;
		cmd_ack[2] = CPU_Rece_Len + 5;
		cmd_ack[3] = NFC_WRITE;
		cmd_ack[4] = NETLINK_SUCCESS;
		memcpy(cmd_ack+5,CPU_Rece_Buff,CPU_Rece_Len);
		send_mg_msg(cmd_ack,cmd_ack[2]);
		kfree(cmd_ack);
	}
	////////////////////////////////////////////////////////////////////////////////////
#else	
	//////////////////////////////////////////////////////////////////////
	//00 A4 04 00 07 D4 10 00 00 14 00 01
	#if 0
	CPU_Send_Len=12;
  	memcpy(CPU_Send_Buff,"\x00\xA4\x04\x00\x07\xD4\x10\x00\x00\x14\x00\x01",CPU_Send_Len);//选择卡号文件
  	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select MF ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750-> 14 OK\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 number 0x%02X\n",CPU_Rece_Buff[i]);
	}
	
	//00 A4 04 00 07 D4 10 00 00 03 00 01
	CPU_Send_Len=12;
  	memcpy(CPU_Send_Buff,"\x00\xA4\x04\x00\x07\xD4\x10\x00\x00\x03\x00\x01",CPU_Send_Len);//选择卡号文件
  	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select MF ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750-> 03 OK\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 number 0x%02X\n",CPU_Rece_Buff[i]);
	}
	#endif
	/////////////////////////////////////////////////////////////////////////
	memcpy(CPU_Send_Buff,"\x01\x23\x32\x14\x56\x65\x47\x89\x98\x7A\xBC\xDE\xFF\xED\xCB\xA0",16);
	result = Tdes( 0x00,CPU_Rece_Buff,CPU_Send_Buff, CPU_Send_Buff+5);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select MF ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750->TdesOK\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 5;i < 13;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}
	
	CPU_Send_Len=5;
	memcpy(CPU_Send_Buff,"\x00\x82\x00\x00\x08",CPU_Send_Len);
	result = APDU_Exchange(13,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select MF ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750-> Exchange OK\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}	
		
		
		

	CPU_Send_Len=7;
	memcpy(CPU_Send_Buff,"\x00\xA4\x00\x00\x02\x00\x1F",CPU_Send_Len);//选择MF文件
	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select MF ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750-> Select MF OK\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}
	
	
	CPU_Send_Len=7;
	memcpy(CPU_Send_Buff,"\x00\xB0\x9F\x00\x00",CPU_Send_Len);//选择MF文件
	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select MF ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750-> Select MF OK\\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}
	
	
	CPU_Send_Len=20;
	memcpy(CPU_Send_Buff,"\x00\xA4\x04\x00\x0E\x32\x50\x41\x59\x2E\x53\x59\x53\x2E\x44\x44\x46\x30\x31\x00",CPU_Send_Len);//选择MF文件
	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select File 1 ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750-> Select File 1 OK\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}

	CPU_Send_Len=14;
	memcpy(CPU_Send_Buff,"\x00\xA4\x04\x00\x08\xA0\x00\x00\x03\x33\x01\x01\x01\x00",CPU_Send_Len);//选择MF文件
	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select File 2 ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750-> Select File 2 OK\n");
	pr_info("nfc_fm17750<- Return = ");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}

	CPU_Send_Len=42;
	memcpy(CPU_Send_Buff,"\x80\xA8\x00\x00\x24\x83\x22\x28\x00\x00\x80\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x01\x56\x00\x00\x00\x00\x00\x01\x56\x13\x08\x28\x82\x12\x34\x56\x78\x00\x00",CPU_Send_Len);//选择MF文件
	result = APDU_Exchange(CPU_Send_Len,CPU_Send_Buff,&CPU_Rece_Len,CPU_Rece_Buff);
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Select File 3 ERROR\n");
		return ERROR;
	}
	pr_info("nfc_fm17750-> Select File 3 OK\n");
	pr_info("nfc_fm17750<- Return = \n");
	for(i = 0;i < CPU_Rece_Len;i ++)
	{
		pr_info("nfc_fm17750 0x%02X\n",CPU_Rece_Buff[i]);
	}
#endif
/*
	result = CPU_Deselect();
	if(result !=OK)
	{
		pr_info("nfc_fm17750-> Deselect ERROR\n");
		//return ERROR;
	}
	pr_info("nfc_fm17750-> Deselect OK\n");
*/	
	return OK;
}