
#include <linux/string.h>
#include "FM175XX_REG.h"
#include "READER_API.h"
#include "FM175XX.h"
#include "MIFARE.h"
#include <linux/delay.h>
#include "fm17750_i2c.h"
struct type_a_struct Type_A; 
struct type_b_struct Type_B;

extern struct i2c_client *fm_client;

void SetTxCRC(unsigned char mode)
{
	if(mode)
			ModifyReg(JREG_TXMODE,0x80,1);
		else
			ModifyReg(JREG_TXMODE,0x80,0);
return;
}

void SetRxCRC(unsigned char mode)
{
	if(mode)
			ModifyReg(JREG_RXMODE,0x80,1);
		else
			ModifyReg(JREG_RXMODE,0x80,0);
	return;
}


/******************************************************************************/
//函数名：Command_Execute
//入口参数：
//			unsigned char* sendbuf:发送数据缓冲区	unsigned char sendlen:发送数据长度
//			unsigned char* recvbuf:接收数据缓冲区	unsigned char* recvlen:接收到的数据长度
//出口参数
/******************************************************************************/
unsigned char Command_Execute(command_struct *comm_status)
{
	unsigned char reg_data;
	unsigned char irq,timeout;
	unsigned char result;
	comm_status->nBitsReceived = 0;
	comm_status->nBytesReceived = 0;
	comm_status->CollPos = 0;
	comm_status->Error = 0;
	
	SetReg(JREG_COMMAND,CMD_IDLE);
	Clear_FIFO();	
	SetReg(JREG_DIVIEN,0x00);
	SetReg(JREG_COMMIEN,0x80);	
	SetReg(JREG_COMMIRQ,0x7F);	
	SetReg(JREG_TMODE,0x80);
    //dev_info(&fm_client->dev, "====%d:%s nBytesToSend:%d\n",__LINE__,__func__,comm_status->nBytesToSend);
	if(comm_status->nBytesToSend > 64)
		return FM175XX_COMM_ERR;//函数不处理大于64字节数据发送
	SPI_Write_FIFO(comm_status->nBytesToSend,comm_status->pExBuf);	
	//dev_info(&fm_client->dev, "====%d:%s Cmd:0x%02x\n",__LINE__,__func__,comm_status->Cmd);
	if(comm_status->Cmd == CMD_TRANSCEIVE)
		{
		SetReg(JREG_COMMAND,comm_status->Cmd);	
		SetReg(JREG_BITFRAMING,0x80 | comm_status->nBitsToSend);
		}
		
	if((comm_status->Cmd == CMD_AUTHENT)||(comm_status->Cmd == CMD_TRANSMIT))
		{			
		SetReg(JREG_BITFRAMING,0x80 | comm_status->nBitsToSend);
		SetReg(JREG_COMMAND,comm_status->Cmd);	
		}
		
	for(timeout = comm_status->Timeout; timeout >0; timeout --)
		{
				mdelay(1);	//延时1ms
				GetReg(JREG_COMMIRQ,&irq);
                //dev_info(&fm_client->dev, "====%d:%s irq:0x%02x,timeout:%d\n",__LINE__,__func__,irq,timeout);
		
				if(irq & 0x02)//ERRIRq = 1
				{
					SetReg(JREG_COMMIRQ,0x02);			
					GetReg(JREG_ERROR,&reg_data);	
					comm_status->Error = reg_data;
					if(comm_status->Error & 0x08)
					{
                        //dev_info(&fm_client->dev, "================111===========\n");
						GetReg(JREG_COLL,&reg_data);	
						comm_status->CollPos = reg_data & 0x1F;//读取冲突位置
						result = FM175XX_COLL_ERR;
						break;	
					}
                    //dev_info(&fm_client->dev, "================222===========\n");
					result = FM175XX_COMM_ERR;
					break;	
				}		
			
				if((irq & 0x40)&&(comm_status->Cmd == CMD_TRANSMIT))//TxIRq = 1
				{
                    //dev_info(&fm_client->dev, "================333===========\n");
					SetReg(JREG_COMMIRQ,0x40);
					result = FM175XX_SUCCESS;
					break;
				}
				
				if((irq & 0x10)&&(comm_status->Cmd == CMD_AUTHENT))//IdleTRq = 1
				{
                    //dev_info(&fm_client->dev, "================444===========\n");
					SetReg(JREG_COMMIRQ,0x10);			
					result = FM175XX_SUCCESS;
					break;	
				}
					
				if((irq & 0x20)&&((comm_status->Cmd == CMD_TRANSCEIVE)||(comm_status->Cmd == CMD_RECEIVE)))//RxIRq = 1
				{
                    //dev_info(&fm_client->dev, "================555===========\n");
					GetReg(JREG_CONTROL,&reg_data);
					comm_status->nBitsReceived = reg_data & 0x07;//接收位长度
				
					GetReg(JREG_FIFOLEVEL,&reg_data);
					comm_status->nBytesReceived = reg_data & 0x3F;//接收字节长度
					if(comm_status->nBytesToReceive != comm_status->nBytesReceived)
					{
                        //dev_info(&fm_client->dev, "================666===========\n");
						result = FM175XX_COMM_ERR;//接收到的数据长度错误
						break;
					}
                    //dev_info(&fm_client->dev, "================777===========\n");
					SPI_Read_FIFO(comm_status->nBytesReceived,comm_status->pExBuf);
					SetReg(JREG_COMMIRQ,0x20);			
					result = FM175XX_SUCCESS;
					break;						
				}
		}
	if(timeout == 0)
			result = FM175XX_TIMER_ERR;
	
	ModifyReg(JREG_BITFRAMING,0x80,0);//CLR Start Send	
	SetReg(JREG_COMMAND,CMD_IDLE);
	return result;
}

void FM175XX_Initial_ReaderA(void)
{
	
	SetReg(JREG_TXMODE,0x00);
	SetReg(JREG_RXMODE,0x00);	
	SetReg(JREG_MODWIDTH,MODWIDTH);				//MODWIDTH 106kbps//0x24 0x26
	SetReg(JREG_GSN,(GSNON<<4)|MODGSNON);	//0x27 0xf8
	SetReg(JREG_CWGSP,GSP);   //0x28 31
	SetReg(JREG_CONTROL,0x10);		//Initiator = 1 //0x0C 
	SetReg(JREG_RFCFG,RXGAIN<<4);   //0x26 0x40
	//SetReg(JREG_RFCFG,0x4f);   //0x26 0x40
    //SetReg(JREG_DEMOD,0x0D);   //0x19 0x0D
	SetReg(JREG_RXTRESHOLD,COLLLEVEL|(MINLEVEL<<4)); //0x18 0x84
    //SetReg(JREG_RXTRESHOLD,0x24); //0x18 0x84
	ModifyReg(JREG_TXAUTO,0x40,1);//Force 100ASK = 1
	Mifare_Clear_Crypto();//循环读卡操作时，需要在读卡前清除前次操作的认证标志
	return;
}


void FM175XX_Initial_ReaderB(void)
{
	ModifyReg(JREG_STATUS2,0x08,0);
	SetReg(JREG_TXMODE,0x80|0x02|0x01);//0x02~0 = 11 :ISO/IEC 14443B
	SetReg(JREG_RXMODE,0x80|0x02|0x01);//0x02~0 = 11 :ISO/IEC 14443B
	SetReg(JREG_TXAUTO,0);
	SetReg(JREG_MODWIDTH,0x26);
	SetReg(JREG_RXTRESHOLD,0x55);
	SetReg(JREG_GSN,0xF8);  
	SetReg(JREG_CWGSP,0x3F);
	SetReg(JREG_MODGSP,0x20);
	SetReg(JREG_CONTROL,0x10);
	SetReg(JREG_RFCFG,0x48);//接收器放大倍数
	return;
}


unsigned char ReaderA_Halt(void)
{
	unsigned char result,buff[2];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;

	SetTxCRC(1);
	SetRxCRC(1);
	command_stauts.pExBuf[0] = 0x50;
  command_stauts.pExBuf[1] = 0x00;
  command_stauts.nBytesToSend = 2;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 0;
  command_stauts.Timeout = 5;//5ms	
	command_stauts.Cmd = CMD_TRANSMIT;	
  result = Command_Execute(&command_stauts);
  return result;
}

unsigned char ReaderA_Wakeup(void)
{
	unsigned char result;
	unsigned char buff[2];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
 	SetTxCRC(0);
	SetRxCRC(0);
	command_stauts.pExBuf[0] = RF_CMD_WUPA;   //
	command_stauts.nBytesToSend = 1;//
	command_stauts.nBitsToSend = 7;
	command_stauts.nBytesToReceive = 2;
	command_stauts.Timeout = 5;//5ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	result = Command_Execute(&command_stauts);
	if((result == FM175XX_SUCCESS)&&(command_stauts.nBytesReceived == 2))
	{
		memcpy(Type_A.ATQA, command_stauts.pExBuf,2);		
	}
	else
		result =  FM175XX_COMM_ERR;
	return result;
}

unsigned char ReaderA_Request(void)
{
	unsigned char result;
	unsigned char buff[2];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
 	SetTxCRC(0);
	SetRxCRC(0);
	command_stauts.pExBuf[0] = RF_CMD_REQA;   //
	command_stauts.nBytesToSend = 1;//
	command_stauts.nBitsToSend = 7;
	command_stauts.nBytesToReceive = 2;
	command_stauts.Timeout = 5;//5ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	result = Command_Execute(&command_stauts);
	if((result == FM175XX_SUCCESS)&&(command_stauts.nBytesReceived == 2))
	{
		memcpy(Type_A.ATQA, command_stauts.pExBuf,2);		
	}
	else
		result =  FM175XX_COMM_ERR;
	return result;
}

//*************************************
//函数  名：ReaderA_AntiColl
//入口参数：size:防冲突等级，包括0、1、2
//出口参数：unsigned char:0:OK  others：错误
//*************************************
unsigned char ReaderA_AntiColl(unsigned char cascade_level)
{
	unsigned char result;
	unsigned char buff[5];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	SetTxCRC(0);
	SetRxCRC(0);
    //dev_info(&fm_client->dev, "===%d:%s==cascade_level = %d \n",__LINE__,__func__,cascade_level);
	if(cascade_level >2)
		return FM175XX_PARAM_ERR;
	command_stauts.pExBuf[0] = RF_CMD_ANTICOL[cascade_level];   //防冲突等级
	command_stauts.pExBuf[1] = 0x20;
	command_stauts.nBytesToSend = 2;						//发送长度：2
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 5;
	command_stauts.Timeout = 5;//5ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	result = Command_Execute(&command_stauts);
	ModifyReg(JREG_COLL,0x80,1);
    //dev_info(&fm_client->dev, "===%d:%s==result = %d,nBytesReceived = %d \n",__LINE__,__func__,result,command_stauts.nBytesReceived);
	if((result == FM175XX_SUCCESS)&&(command_stauts.nBytesReceived == 5))
	{
		memcpy(Type_A.UID +(cascade_level*4),command_stauts.pExBuf,4);
		memcpy(Type_A.BCC + cascade_level,command_stauts.pExBuf + 4,1);
		if((Type_A.UID[cascade_level*4] ^  Type_A.UID[cascade_level*4 + 1] ^ Type_A.UID[cascade_level*4 + 2]^ Type_A.UID[cascade_level*4 + 3] ^ Type_A.BCC[cascade_level]) !=0)
				result = FM175XX_COMM_ERR;
	}
	return result;
}

//*************************************
//函数  名：ReaderA_Select
//入口参数：size:防冲突等级，包括0、1、2
//出口参数：unsigned char:0:OK  others：错误
//*************************************
unsigned char ReaderA_Select(unsigned char cascade_level)
{
	unsigned char result;	
	unsigned char buff[7];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	SetTxCRC(1);
	SetRxCRC(1);
	if(cascade_level > 2)							 //最多三重防冲突
		return FM175XX_PARAM_ERR;
	*(command_stauts.pExBuf+0) = RF_CMD_ANTICOL[cascade_level];   //防冲突等级
	*(command_stauts.pExBuf+1) = 0x70;   					//
	*(command_stauts.pExBuf+2) = Type_A.UID[4*cascade_level];
	*(command_stauts.pExBuf+3) = Type_A.UID[4*cascade_level+1];
	*(command_stauts.pExBuf+4) = Type_A.UID[4*cascade_level+2];
	*(command_stauts.pExBuf+5) = Type_A.UID[4*cascade_level+3];
	*(command_stauts.pExBuf+6) = Type_A.BCC[cascade_level];
	command_stauts.nBytesToSend = 7;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 1;
	command_stauts.Timeout = 5;//5ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	result = Command_Execute(&command_stauts);
	if((result == FM175XX_SUCCESS)&&(command_stauts.nBytesReceived == 1))
		Type_A.SAK [cascade_level] = *(command_stauts.pExBuf);
	else
		result = FM175XX_COMM_ERR;
	return result;
}



void SetRf(unsigned char mode)
{
	if(mode == 0)	
		{
		ModifyReg(JREG_TXCONTROL,0x01|0x02,0);
		}
	if(mode == 1)	
		{
		ModifyReg(JREG_TXCONTROL,0x01,1);
		ModifyReg(JREG_TXCONTROL,0x02,0);	
		}
	if(mode == 2)	
		{
		ModifyReg(JREG_TXCONTROL,0x01,0);
		ModifyReg(JREG_TXCONTROL,0x02,1);	
		}
	if(mode == 3)	
		{
		ModifyReg(JREG_TXCONTROL,0x01|0x02,1);
		}
		mdelay(10);
}

unsigned char ReaderA_CardActivate(void)
{
unsigned char  result,cascade_level;	
		result = ReaderA_Wakeup();//
		if (result != FM175XX_SUCCESS)
			return FM175XX_COMM_ERR;
			//dev_info(&fm_client->dev, "===11  ATQA:0x%02x,0x%02x====\n",Type_A.ATQA[0],Type_A.ATQA[1]);
			if 	((Type_A.ATQA[0]&0xC0)==0x00)	//1重UID		
				Type_A.CASCADE_LEVEL = 1;
			
			if 	((Type_A.ATQA[0]&0xC0)==0x40)	//2重UID			
				Type_A.CASCADE_LEVEL = 2;		

			if 	((Type_A.ATQA[0]&0xC0)==0x80)	//3重UID
				Type_A.CASCADE_LEVEL = 3;				
			
			for (cascade_level = 0;cascade_level < Type_A.CASCADE_LEVEL;cascade_level++)
				{
					result = ReaderA_AntiColl(cascade_level);//
					if (result != FM175XX_SUCCESS)
                    {
                        dev_info(&fm_client->dev, "===22  cascade_level:%d====\n",cascade_level);
						return FM175XX_COMM_ERR;
                    }
					
					result=ReaderA_Select(cascade_level);//
					if (result != FM175XX_SUCCESS)
                    {
                        dev_info(&fm_client->dev, "===33  cascade_level:%d====\n",cascade_level);
						return FM175XX_COMM_ERR;
                    }
				}						
			
		return result;
}

unsigned char ReaderB_Wakeup(void)
{
	unsigned char result;
	unsigned char buff[12];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
 	SetTxCRC(1);
	SetRxCRC(1);
	command_stauts.pExBuf[0] = 0x05;  //APf
	command_stauts.pExBuf[1] = 0x00;   //AFI (00:for all cards)
	command_stauts.pExBuf[2] = 0x08;   //PARAM(REQB,Number of slots =0)
	command_stauts.nBytesToSend = 3;//
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 12;
	command_stauts.Timeout = 5;//5ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	result = Command_Execute(&command_stauts);
	if((result == FM175XX_SUCCESS)&&(command_stauts.nBytesReceived == 12))
	{
		memcpy(Type_B.ATQB, command_stauts.pExBuf,12);	
		memcpy(Type_B.PUPI,Type_B.ATQB + 1,4);
		memcpy(Type_B.APPLICATION_DATA,Type_B.ATQB + 6,4);
		memcpy(Type_B.PROTOCOL_INF,Type_B.ATQB + 10,3);
	}
	else
		result = FM175XX_COMM_ERR;
	return result;
}

unsigned char ReaderB_Request(void)
{
	unsigned char result;
	unsigned char buff[12];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
 	SetTxCRC(1);
	SetRxCRC(1);
	command_stauts.pExBuf[0] = 0x05;  //APf
	command_stauts.pExBuf[1] = 0x00;   //AFI (00:for all cards)
	command_stauts.pExBuf[2] = 0x00;   //PARAM(REQB,Number of slots =0)
	command_stauts.nBytesToSend = 3;//
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 12;
	command_stauts.Timeout = 5;//5ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	result = Command_Execute(&command_stauts);
	if((result == FM175XX_SUCCESS)&&(command_stauts.nBytesReceived == 12))
	{
		memcpy(Type_B.ATQB, command_stauts.pExBuf,12);	
		memcpy(Type_B.PUPI,Type_B.ATQB + 1,4);
		memcpy(Type_B.APPLICATION_DATA,Type_B.ATQB + 6,4);
		memcpy(Type_B.PROTOCOL_INF,Type_B.ATQB + 10,3);
	}
	else
		result = FM175XX_COMM_ERR;
	return result;
}

unsigned char ReaderB_Attrib(void)
{
	unsigned char result;
	unsigned char buff[9];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
 	SetTxCRC(1);
	SetRxCRC(1);
	command_stauts.pExBuf[0] = 0x1D;  //
	command_stauts.pExBuf[1] = Type_B.PUPI[0];   //
	command_stauts.pExBuf[2] = Type_B.PUPI[1];   //
	command_stauts.pExBuf[3] = Type_B.PUPI[2];   //
	command_stauts.pExBuf[4] = Type_B.PUPI[3];   //	
	command_stauts.pExBuf[5] = 0x00;  //Param1
	command_stauts.pExBuf[6] = 0x08;  //01--24,08--256
	command_stauts.pExBuf[7] = 0x01;  //COMPATIBLE WITH 14443-4
	command_stauts.pExBuf[8] = 0x01;  //CID:01 
	command_stauts.nBytesToSend = 9;//
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 1;
	command_stauts.Timeout = 5;//5ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	result = Command_Execute(&command_stauts);
	if(result == FM175XX_SUCCESS)
	{
		Type_B.LEN_ATTRIB = command_stauts.nBytesReceived;
		memcpy(Type_B.ATTRIB, command_stauts.pExBuf,Type_B.LEN_ATTRIB);		
	}
	else
		result = FM175XX_COMM_ERR;
	return result;
}

unsigned char FM175XX_Polling(unsigned char *polling_card)
{
unsigned char result;
	*polling_card = 0;
	FM175XX_Initial_ReaderA();
	result = ReaderA_Wakeup();//
		if (result == FM175XX_SUCCESS)	
			*polling_card |= 0x01;
	FM175XX_Initial_ReaderB();
	result = ReaderB_Wakeup();//
		if (result == FM175XX_SUCCESS)
			*polling_card |= 0x02;
if (*polling_card !=0)		
	return 0;	
else
	return 1;
}

