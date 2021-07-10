/*************************************************************/
//2014.07.15�޸İ�
/*************************************************************/
#include "spi.h"
#include "fm175xx.h"
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>

#define Fm175xx_Debug 0

extern int nfc_reset_gpio;

extern struct i2c_client *fm_client;

/*!
 * ��������	    Read_Reg
 * ���ܣ�	    ���Ĵ�������
 * ���������	reg_add���Ĵ�����ַ
 * ����ֵ��	    �Ĵ�����ֵ
*/
unsigned char Read_Reg(unsigned char reg_add)
{
	unsigned char reg_value;
	reg_value=SPIRead(reg_add);
	return reg_value;
}

/*!
 * ��������	    Read_Reg All
 * ���ܣ�	    ��64���Ĵ�������
 * ���������	��
 * �������		*reg_value���Ĵ�����ֵָ��,����Ϊ64�ֽ�
 * ����ֵ��	    OK
*/
unsigned char Read_Reg_All(unsigned char *reg_value)
{
	unsigned char i;
	for (i=0;i < 64;i++)
		*(reg_value+i)=Read_Reg(i);
	return OK;
}

/*!
 * ��������	    Write_Reg									 
 * ���ܣ�	    д�Ĵ�������								 
 * 															 
 * ���������	reg_add���Ĵ�����ַ��reg_value���Ĵ�����ֵ	 
 * ����ֵ��	    OK											 
 * 				ERROR										 
*/
unsigned char Write_Reg(unsigned char reg_add,unsigned char reg_value)
{
	SPIWrite(reg_add,reg_value);
	return OK;
}

void ModifyReg(unsigned char addr,unsigned char mask,unsigned char set)
{
	unsigned char regdata;
	
	regdata = Read_Reg(addr);
	
	if(set)	{
		regdata |= mask;
	}else{
		regdata &= ~mask;
	}

	Write_Reg(addr,regdata);
	return ;
}

/*!
 * ��������	    Read_FIFO
 * ���ܣ�	    ��ȡFIFO 
   
 * ���������	length����ȡFIFO���ݳ��� 
 * ���������	*fifo_data��FIFO���ݴ��ָ��		 
 * ����ֵ:
 * 										 
*/
void Read_FIFO(unsigned char length,unsigned char *fifo_data)
{
	SPIRead_Sequence(length,FIFODataReg,fifo_data);
	return;
}

/*!
 * ��������	    Write_FIFO									 
 * ���ܣ�	    д��FIFO  									 
 * 															 
 * ���������	length����ȡ���ݳ��ȣ�						 
 * 				*fifo_data�����ݴ��ָ��					 
 * ���������												 
 * ����ֵ��	    											 
 * 															 
*/
void Write_FIFO(unsigned char length,unsigned char *fifo_data)
{
	SPIWrite_Sequence(length,FIFODataReg,fifo_data);
	return;
}

/*!
 * ��������	    Clear_FIFO
 * ���ܣ�	    ���FIFO  
 * 						
 * ���������	��		
 * ���������			
 * ����ֵ��	    OK		
 * 				ERROR	
*/
unsigned char Clear_FIFO(void)
{
	Set_BitMask(FIFOLevelReg,0x80);//���FIFO����
	if (Read_Reg(FIFOLevelReg) == 0)
		return OK;
	else
		return ERROR;
}

/*!
 * ��������	    Set_BitMask							
 * ���ܣ�	    ��λ�Ĵ�������						
 * ���������	reg_add���Ĵ�����ַ��mask���Ĵ�����λ
 * ����ֵ��	    OK									
 * 				ERROR								
*/
unsigned char Set_BitMask(unsigned char reg_add,unsigned char mask)
{
	unsigned char result;
	result=Write_Reg(reg_add,Read_Reg(reg_add) | mask);  // set bit mask
	return result;
}

/*!
 * ��������	    Clear_BitMask
 * ���ܣ�	    ���λ�Ĵ�������
 * 				
 * ���������	reg_add���Ĵ�����ַ��mask���Ĵ������λ
 * ����ֵ��	    OK
 * 				ERROR
*/
unsigned char Clear_BitMask(unsigned char reg_add,unsigned char mask)
{
	unsigned char result;
	result=Write_Reg(reg_add,Read_Reg(reg_add) & ~mask);  // clear bit mask
	return result;
}

/*!
 * ��������	    Set_RF
 * ���ܣ�	    ������Ƶ���
 	 			
 * ���������	mode����Ƶ���ģʽ
 	 			0���ر����
 	 			1,����TX1���
 	 			2,����TX2���
 	 			3��TX1��TX2�������TX2Ϊ�������
 * ����ֵ��	    OK
				ERROR
*/
unsigned char Set_Rf(unsigned char mode)
{
	unsigned char result;
	
	if((Read_Reg(TxControlReg)&0x03) == mode)
		return OK;
	
	if(mode == 0)
	{
		result=Clear_BitMask(TxControlReg,0x03); //�ر�TX1��TX2���
	}
	else if(mode == 1)
	{
		result=Clear_BitMask(TxControlReg,0x01); //����TX1���
	}
	else if(mode == 2)
	{
		result=Clear_BitMask(TxControlReg,0x02); //����TX2���
	}
	else if(mode == 3)
	{
		result=Set_BitMask(TxControlReg,0x03); //��TX1��TX2���
	}
	//msleep(200);//��TX�������Ҫ��ʱ�ȴ������ز��ź��ȶ�
	return result;
}

int delay_1ms(void)
{
	int i,j;
	
	i = 0;
	for (i = 0; i < 100; i ++) {
		j = i;
	}
	
	return j;
}

/*!
 * ��������	    Pcd_Comm
 * ���ܣ�	    ������ͨ��
  				
 * ���������	Command��ͨ�Ų������
  				pInData�������������飻
  				InLenByte���������������ֽڳ��ȣ�
  				pOutData�������������飻
  				pOutLenBit���������ݵ�λ����
 * ����ֵ��	    OK
 				ERROR
*/  
unsigned char Pcd_Comm(	unsigned char Command, 
                 		unsigned char *pInData, 
                 		unsigned char InLenByte,
                 		unsigned char *pOutData, 
                 		unsigned int *pOutLenBit)
{
	unsigned char result,reg_data;
	unsigned char rx_temp=0;//��ʱ�����ֽڳ���
	unsigned char rx_len=0;//���������ֽڳ���
	unsigned char lastBits=0;//��������λ����
	unsigned char irq;
	int i = 0,j = 0;
	*pOutLenBit =0;
	Clear_FIFO();
	Write_Reg(CommandReg,Idle);
   	Write_Reg(WaterLevelReg,0x20);//����FIFOLevel=32�ֽ�
	Write_Reg(ComIrqReg,0x7F);//���IRQ��־
 	if(Command == MFAuthent)
	{
		Write_FIFO(InLenByte,pInData);//������֤��Կ
		Set_BitMask(BitFramingReg,0x80);//��������
	}
	Set_BitMask(TModeReg,0x80);//�Զ�������ʱ��

 	Write_Reg(CommandReg,Command);

	delay_1ms();
	
	while(1)//ѭ���ж��жϱ�ʶ
	{
		j ++;
		if(j > 10)
			break;
		irq = Read_Reg(ComIrqReg);//��ѯ�жϱ�־
		if(irq&0x01)	//TimerIRq  ��ʱ��ʱ���þ�
		{
			result=TIMEOUT_Err;		
			break;
		}
		
		if(Command == MFAuthent)
		{
			if(irq&0x10)	//IdelIRq  command�Ĵ���Ϊ���У�ָ��������
			{
				result=OK;
		  		break;
			}
	   	}

		if(Command==Transmit)	
		{
			if((irq & 0x04) && (InLenByte > 0))	//LoAlertIrq+�����ֽ�������0
			{
				if (InLenByte < 32)
				{
					Write_FIFO(InLenByte,pInData);	
					InLenByte=0;
				}
				else
				{
					Write_FIFO(32,pInData);
					InLenByte=InLenByte-32;
					pInData=pInData+32;
				}
				Set_BitMask(BitFramingReg,0x80);	//��������
				Write_Reg(ComIrqReg,0x04);	//���LoAlertIrq
			}
			
			if((irq&0x40)&&(InLenByte==0))	//TxIRq
			{
				result=OK;
			  	break;
			}
		}
		
		if(Command == Transceive)
		{
			if((irq & 0x04) && (InLenByte > 0))	//LoAlertIrq + �����ֽ�������0
			{	
				if (InLenByte > 32)
				{
					Write_FIFO(32,pInData);
					InLenByte=InLenByte-32;
					pInData=pInData+32;
				}
				else
				{
					Write_FIFO(InLenByte,pInData);
					InLenByte=0;
				}
				Set_BitMask(BitFramingReg,0x80);//��������
				Write_Reg(ComIrqReg,0x04);//���LoAlertIrq
			}
			if(irq&0x08)	//HiAlertIRq
			{
				if((irq&0x40)&&(InLenByte == 0)&&(Read_Reg(FIFOLevelReg)>32))//TxIRq	+ �����ͳ���Ϊ0 + FIFO���ȴ���32
				{
					Read_FIFO(32,pOutData+rx_len); //����FIFO����
					rx_len=rx_len+32;
					Write_Reg(ComIrqReg,0x08);	//��� HiAlertIRq
				}
			}
			if((irq&0x20)&&(InLenByte == 0))	//RxIRq=1
			{
				result=OK;
				break;
			}
		}
	}
	
	if (Command == Transceive)
	{
		rx_temp=Read_Reg(FIFOLevelReg);
		lastBits = Read_Reg(ControlReg) & 0x07;

		#if Fm175xx_Debug
		pr_info("nfc_fm17750<- rx_temp =  0x%02X\n",rx_temp);
		pr_info("nfc_fm17750<- rx_len =  0x%02X\n",rx_len);
		pr_info("nfc_fm17750<- lastBits =  0x%02X\n",lastBits);
		#endif
		
		if ((rx_temp == 0)&&(lastBits > 0))//����յ�����δ��1���ֽڣ������ý��ճ���Ϊ1���ֽ�.
			rx_temp=1;
			
		Read_FIFO(rx_temp,pOutData+rx_len); //����FIFO����
	   
		rx_len=rx_len+rx_temp;//���ճ����ۼ�
		
		#if Fm175xx_Debug
		pr_info("nfc_fm17750<- FIFO = ");
		for(i = 0;i < rx_len;i ++)
		{
			pr_info("nfc_fm17750 0x%02X\n",pOutData[i]);
		}
		#endif
		
		if (lastBits > 0)
			*pOutLenBit = (rx_len-1)*(unsigned int)8 + lastBits;
		else
			*pOutLenBit = rx_len*(unsigned int)8;
	}
	
	if(result == OK)
		result=Read_Reg(ErrorReg);//ErrorReg��ֵ��ΪPcd_Comm�����ķ���ֵ���ɺ��������д��������
	
	#if Fm175xx_Debug
		reg_data=Read_Reg(ErrorReg);
		pr_info("nfc_fm17750<- ErrorReg = 0x%02X\n",reg_data);
	#endif
	
	Set_BitMask(ControlReg,0x80);     // stop timer now
	Write_Reg(CommandReg,Idle); 
 	Clear_BitMask(BitFramingReg,0x80);//�رշ���
	return result;
}

/*!
 * ��������	    Pcd_SetTimer
 * ���ܣ�	    ���ý�����ʱ
 * ���������	delaytime����ʱʱ�䣨��λΪ���룩
 * ����ֵ��	    OK
*/
 unsigned char Pcd_SetTimer(unsigned int delaytime)//�趨��ʱʱ�䣨ms��
{
	unsigned long TimeReload;
	unsigned int Prescaler;

	Prescaler=0;
	TimeReload=0;
	while(Prescaler < 0xfff)
	{
		TimeReload = ((delaytime*(long)13560)-1)/(Prescaler*2+1);
		
		if( TimeReload < 0xffff)
			break;
		Prescaler++;
	}
	
	TimeReload=TimeReload & 0xFFFF;
	Set_BitMask(TModeReg,Prescaler >> 8);
	Write_Reg(TPrescalerReg,Prescaler & 0xFF);					
	Write_Reg(TReloadMSBReg,TimeReload >> 8);
	Write_Reg(TReloadLSBReg,TimeReload & 0xFF);
	
	return OK;
}

/*!
 * ��������	    Pcd_ConfigISOType
 * ���ܣ�	    ���ò���Э��
 * ���������	type 0��ISO14443AЭ�飻
 * 					 1��ISO14443BЭ�飻
 * ����ֵ��	    OK
*/
unsigned char Pcd_ConfigISOType(unsigned char type)
{
	if (type == 0)                     //ISO14443_A
	{
		Set_BitMask(ControlReg, 0x10); //ControlReg 0x0C ����readerģʽ
		Set_BitMask(TxAutoReg, 0x40); //TxASKReg 0x15 ����100%ASK��Ч
		Write_Reg(TxModeReg, 0x00);  //TxModeReg 0x12 ����TX CRC��Ч��TX FRAMING =TYPE A
		Write_Reg(RxModeReg, 0x00); //RxModeReg 0x13 ����RX CRC��Ч��RX FRAMING =TYPE A

	   	Write_Reg(GsNOnReg,0xF1);
		Write_Reg(CWGsPReg,0x3F);
		Write_Reg(ModGsPReg,0x01);

		Write_Reg(RFCfgReg,0x40);	//Bit6~Bit4 ��������
//		Write_Reg(DemodReg,0x0D);
		Write_Reg(RxThresholdReg,0x84);//0x18�Ĵ���	Bit7~Bit4 MinLevel Bit2~Bit0 CollLevel
	   	Write_Reg(AutoTestReg,0x40);//AmpRcv=1
	}
	else if (type == 1)                     //ISO14443_B
	{
		Write_Reg(ControlReg, 0x10); //ControlReg 0x0C ����readerģʽ
		Write_Reg(TxModeReg, 0x83); //TxModeReg 0x12 ����TX CRC��Ч��TX FRAMING =TYPE B
		Write_Reg(RxModeReg, 0x83); //RxModeReg 0x13 ����RX CRC��Ч��RX FRAMING =TYPE B
		Write_Reg(TxAutoReg, 0x00);// TxASKReg 0x15 ����100%ASK��Ч
	}
	else if (type == 2)                     //Felica
	{
		Write_Reg(ControlReg, 0x10); //ControlReg 0x0C ����readerģʽ
		Write_Reg(TxModeReg, 0x92); //TxModeReg 0x12 ����TX CRC��Ч��212kbps,TX FRAMING =Felica
		Write_Reg(RxModeReg, 0x96); //RxModeReg 0x13 ����RX CRC��Ч��212kbps,Rx Multiple Enable,RX FRAMING =Felica
		Write_Reg(GsNOnReg, 0xF4); //GsNReg 0x27 ����ON�絼
		Write_Reg(CWGsPReg, 0x20); //
		Write_Reg(GsNOffReg, 0x4F); //GsNOffReg 0x23 ����OFF�絼
		Write_Reg(ModGsPReg, 0x20);
		Write_Reg(TxAutoReg, 0x07);// TxASKReg 0x15 ����100%ASK��Ч
	}
	
	return OK;
}

/*!
 * ��������	    FM175XX_SoftReset
 * ���ܣ�	    ��λ����
  				
 * ���������	
  				
   
 * ����ֵ��	    OK
				ERROR
*/
unsigned char  FM175XX_SoftReset(void)
{
	unsigned char reg_data=0;
	Write_Reg(CommandReg,SoftReset);
	msleep(1);
	reg_data = Read_Reg(CommandReg);
	if(reg_data == Idle)
		return OK;
	else
		return ERROR;
}

unsigned char FM175XX_HardReset(void)
{
	unsigned char reg_data=0;

	//pr_info("%d/%s\n",__LINE__,__FUNCTION__);
	gpio_set_value(nfc_reset_gpio,1);//NPD=0
	msleep(10);
	gpio_set_value(nfc_reset_gpio,0);//NPD=1
	msleep(100);
	reg_data=Read_Reg(CommandReg);
	//pr_info("%d/%s reg_data:%d\n",__LINE__,__FUNCTION__,reg_data);
	if(reg_data == Idle)
		return OK;
	else
	{
		pr_info("FM175XX_HardReset failed reg_data.1 = %d\n",reg_data);
		return ERROR;
	}
}

/*!
 * ��������	    FM175XX_SoftPowerdown
 * ���ܣ�	    ����͹��Ĳ���
 * ���������	
 * ����ֵ��	    OK������͹���ģʽ��
 * 				ERROR���˳��͹���ģʽ��
*/
unsigned char FM175XX_SoftPowerdown(void)
{
	if (Read_Reg(CommandReg)&0x10)
	{
		Clear_BitMask(CommandReg,0x10);//�˳��͹���ģʽ
		return ERROR;
	}
	else
		Set_BitMask(CommandReg,0x10);//����͹���ģʽ
	return OK;
}

/*!
 * ��������	    FM175XX_HardPowerdown
 * ���ܣ�	    Ӳ���͹��Ĳ���
 * ���������	
 * ����ֵ��	    OK������͹���ģʽ(mode=1)��
 * 				ERROR���˳��͹���ģʽ(mode=0)��
*/
unsigned char FM175XX_HardPowerdown(unsigned char mode)
{	
	if (mode == 0)
	{
		gpio_set_value(nfc_reset_gpio,0);//NPD=1
		msleep(20);
		if(gpio_get_value(nfc_reset_gpio))
			return ERROR;
	}
	if(mode == 1)
	{
		gpio_set_value(nfc_reset_gpio,1);//NPD=0
		msleep(20);
		if(!gpio_get_value(nfc_reset_gpio))
			return ERROR;
	}
	return OK; //����͹���ģʽ
}

/*!
 * ��������	    Read_Ext_Reg
 * ���ܣ�	    ��ȡ��չ�Ĵ���
 * ���������	reg_add���Ĵ�����ַ
 * ����ֵ��	    �Ĵ�����ֵ
*/
unsigned char Read_Ext_Reg(unsigned char reg_add)
{
 	Write_Reg(0x0F,0x80+reg_add);
 	return Read_Reg(0x0F);
}
/*!
*��������	Write_Ext_Reg
*���ܣ�	    д����չ�Ĵ���
*���������	reg_add���Ĵ�����ַ��reg_value���Ĵ�����ֵ
*����ֵ��	OK
*			ERROR
*/
unsigned char Write_Ext_Reg(unsigned char reg_add,unsigned char reg_value)
{
	Write_Reg(0x0F,0x40+reg_add);
	Write_Reg(0x0F,0xC0+reg_value);
	return OK;
}


void GetReg(unsigned char addr,unsigned char *regdata)
{
	  fm_i2c_read(fm_client, addr, regdata, 1);
    	return ;
}

void SetReg(unsigned char addr,unsigned char regdata)
{

    fm_i2c_write(fm_client, addr, &regdata, 1);
	return ;
}


void SPI_Write_FIFO(unsigned char reglen,unsigned char* regbuf)  //SPI�ӿ�����дFIFO�Ĵ��� 
{

    fm_i2c_write(fm_client, 0x09, regbuf, reglen);

	return;
}

void SPI_Read_FIFO(unsigned char reglen,unsigned char* regbuf)  //SPI�ӿ�������FIFO
{

    fm_i2c_read(fm_client, 0x09, regbuf, reglen);

	return;
}


unsigned char Set_Ext_BitMask(unsigned char reg_add,unsigned char mask)
{
	unsigned char result;
	result=Write_Ext_Reg(reg_add,Read_Ext_Reg(reg_add) | mask);  // set bit mask
	return result;
}

unsigned char Clear_Ext_BitMask(unsigned char reg_add,unsigned char mask)
{
	unsigned char result;
	result=Write_Ext_Reg(reg_add,Read_Ext_Reg(reg_add) & (~mask));  // clear bit mask
	return result;
} 
