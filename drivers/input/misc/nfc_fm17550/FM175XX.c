
#include "fm17750_i2c.h"
#include "FM175XX_REG.h"
extern struct i2c_client *fm_client;
extern int nfc_reset_gpio;
void FM175XX_HPD(unsigned char mode)//mode = 1 �˳�HPDģʽ ��mode = 0 ����HPDģʽ
{
#if 0
	if(mode == 0x0)
		{
		GPIO_ResetBits(PORT_NRST,PIN_NRST);//NPD = 0	����NPDģʽ

		GPIO_InitStructure.GPIO_Pin = PIN_SCK | PIN_MISO | PIN_MOSI; 
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //GPIO����Ϊ���״̬
		GPIO_Init(PORT_SPI, &GPIO_InitStructure); 
			
		GPIO_ResetBits(PORT_SPI,PIN_SCK | PIN_MISO | PIN_MOSI);//SCK = 0��MISO = 0��MOSI = 0
			
		GPIO_InitStructure.GPIO_Pin = PIN_NSS;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
		GPIO_Init(PORT_SPI, &GPIO_InitStructure);	
		GPIO_SetBits(PORT_SPI,PIN_NSS);	//NSS = 1;
			
		}
	else
		{
		GPIO_SetBits(PORT_NRST,PIN_NRST);	//NPD = 1 �˳�HPDģʽ
		mdelay(1);//��ʱ1ms���ȴ�FM175XX����
			
		GPIO_InitStructure.GPIO_Pin = PIN_SCK | PIN_MISO | PIN_MOSI; 
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;//GPIO����ΪSPI���� 
		GPIO_Init(PORT_SPI, &GPIO_InitStructure);  
			
		GPIO_InitStructure.GPIO_Pin = PIN_NSS;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
		GPIO_Init(PORT_SPI, &GPIO_InitStructure);	
		GPIO_SetBits(PORT_SPI,PIN_NSS);	//NSS = 1;		
			
		}
#endif
    if(mode == 0x0)
        gpio_set_value(nfc_reset_gpio,0);
    else
        gpio_set_value(nfc_reset_gpio,1);
	return;
}


//***********************************************
//�������ƣ�Read_Reg(unsigned char addr,unsigned char *regdata)
//�������ܣ���ȡ�Ĵ���ֵ
//��ڲ�����addr:Ŀ��Ĵ�����ַ   regdata:��ȡ��ֵ
//���ڲ�����unsigned char  TRUE����ȡ�ɹ�   FALSE:ʧ��
//***********************************************
void GetReg(unsigned char addr,unsigned char *regdata)
{
#if 0
		GPIO_ResetBits(PORT_SPI,PIN_NSS);	//NSS = 0;
		addr = (addr << 1) | 0x80;	   
		
		SPI_SendData(SPI2,addr);   /* Send SPI1 data */ 
		//while(SPI_GetFlagStatus(SPI1, SPI_FLAG_TXE)==0);	  /* Wait for SPI1 Tx buffer empty */ 
		while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE)==0);
		SPI_ClearFlag(SPI2,SPI_FLAG_RXNE);	
		*regdata = SPI_ReceiveData(SPI2);		 /* Read SPI1 received data */
		SPI_ClearFlag(SPI2, SPI_FLAG_RXNE); 
		SPI_SendData(SPI2,0x00);   /* Send SPI1 data */
		//while(SPI_GetFlagStatus(SPI1, SPI_FLAG_TXE)==0)      
		while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE)==0);
		SPI_ClearFlag(SPI2,SPI_FLAG_RXNE);	
		*regdata = SPI_ReceiveData(SPI2);		 /* Read SPI1 received data */

		GPIO_SetBits(PORT_SPI,PIN_NSS);	//NSS = 1;	
#endif
    fm_i2c_read(fm_client, addr, regdata, 1);
	return ;
}

//***********************************************
//�������ƣ�Write_Reg(unsigned char addr,unsigned char* regdata)
//�������ܣ�д�Ĵ���
//��ڲ�����addr:Ŀ��Ĵ�����ַ   regdata:Ҫд���ֵ
//���ڲ�����unsigned char  TRUE��д�ɹ�   FALSE:дʧ��
//***********************************************
void SetReg(unsigned char addr,unsigned char regdata)
{
#if 0
			GPIO_ResetBits(PORT_SPI,PIN_NSS);	//NSS = 0;
			addr = (addr << 1) & 0x7F;	 	   
	
			SPI_SendData(SPI2,addr);   /* Send SPI1 data */ 
			//while(SPI_GetFlagStatus(SPI1, SPI_FLAG_TXE)==0);	  /* Wait for SPI1 Tx buffer empty */ 
			while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE)==0);
			SPI_ClearFlag(SPI2,SPI_FLAG_RXNE);	
			SPI_ReceiveData(SPI2);		 /* Read SPI1 received data */; 
			SPI_SendData(SPI2,regdata);   /* Send SPI1 data */
			//while(SPI_GetFlagStatus(SPI1, SPI_FLAG_TXE)==0);		
    	while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE)==0);
			SPI_ClearFlag(SPI2,SPI_FLAG_RXNE);	
			SPI_ReceiveData(SPI2);		 /* Read SPI1 received data */
	
			GPIO_SetBits(PORT_SPI,PIN_NSS);	//NSS = 1;
#endif
    fm_i2c_write(fm_client, addr, &regdata, 1);
	return ;
}

//*******************************************************
//�������ƣ�ModifyReg(unsigned char addr,unsigned char* mask,unsigned char set)
//�������ܣ�д�Ĵ���
//��ڲ�����addr:Ŀ��Ĵ�����ַ   mask:Ҫ�ı��λ  
//         set:  0:��־��λ����   ����:��־��λ����
//���ڲ�����
//********************************************************
void ModifyReg(unsigned char addr,unsigned char mask,unsigned char set)
{
	unsigned char regdata;
	
	GetReg(addr,&regdata);
	
		if(set)
		{
			regdata |= mask;
		}
		else
		{
			regdata &= ~mask;
		}

	SetReg(addr,regdata);
	return ;
}

void SPI_Write_FIFO(u8 reglen,u8* regbuf)  //SPI�ӿ�����дFIFO�Ĵ��� 
{
#if 0
	u8  i;
	u8 regdata;

	GPIO_ResetBits(PORT_SPI,PIN_NSS);	//NSS = 0;
	SPI_SendData(SPI2,0x12);   /* Send FIFO addr 0x09<<1 */ 
	while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE) == 0);
	regdata = SPI_ReceiveData(SPI2);		 /* not care data */
	for(i = 0;i < reglen;i++)
	{
		regdata = *(regbuf+i);	  //RegData_i
		SPI_SendData(SPI2,regdata);   /* Send addr_i i��1*/
		while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE) == 0);
    	regdata = SPI_ReceiveData(SPI2);		 /* not care data */
	}
	GPIO_SetBits(PORT_SPI,PIN_NSS);	//NSS = 1;
#endif
    fm_i2c_write(fm_client, 0x09, regbuf, reglen);
/*    int i = 0;
    for (i = 0;i < reglen;i ++)
    {
        fm_i2c_write(fm_client, 0x09+i, regbuf+i, 1);
    }*/
	return;
}

void SPI_Read_FIFO(u8 reglen,u8* regbuf)  //SPI�ӿ�������FIFO
{
#if 0
	u8  i;

	GPIO_ResetBits(PORT_SPI,PIN_NSS);	//NSS = 0;
	SPI_SendData(SPI2,0x92);   /* Send FIFO addr 0x09<<1|0x80 */ 
	while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE)==0);
	*(regbuf) = SPI_ReceiveData(SPI2);		 /* not care data */
	
	for(i=1;i<reglen;i++)
	{
		SPI_SendData(SPI2,0x92);   /* Send FIFO addr 0x09<<1|0x80 */ 
		while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE)==0);
		*(regbuf+i-1) = SPI_ReceiveData(SPI2);  //Data_i-1
	} 
	SPI_SendData(SPI2,0x00);   /* Send recvEnd data 0x00 */      
    while(SPI_GetFlagStatus(SPI2, SPI_FLAG_RXNE)==0);
    *(regbuf+i-1) = SPI_ReceiveData(SPI2);		 /* Read SPI1 received data */

	GPIO_SetBits(PORT_SPI,PIN_NSS);	//NSS = 1;
#endif
    fm_i2c_read(fm_client, 0x09, regbuf, reglen);
/*    int i  = 0;
    for(i = 0;i < reglen;i ++)
    {
        fm_i2c_read(fm_client, 0x09+i, regbuf+i, 1);
    }*/
	return;
}


void Clear_FIFO(void)
{
	u8 regdata;
		
	GetReg(JREG_FIFOLEVEL,&regdata);
	if((regdata & 0x7F) != 0)			//FIFO������գ���FLUSH FIFO
	{
	SetReg(JREG_FIFOLEVEL,JBIT_FLUSHFIFO);
	}
	return ;
}

void FM175XX_HardReset(void)
{
#if 0
	unsigned char reg_data[20],i;
	
	//Uart_Send_Msg("HARD 0\r\n");
    dev_info(&fm_client->dev, "NFC HARD RESET\n");
	GPIO_SetBits(PORT_SPI,PIN_NSS);//NSS = 1
	GPIO_ResetBits(PORT_NRST,PIN_NRST);//NPD = 0	����NPDģʽ
	mdelay(10);		
	
	GPIO_SetBits(PORT_NRST,PIN_NRST);	//NPD = 1 
//	for(i=0;i<100;i++)
//	{
//	GetReg(JREG_COMMAND,&reg_data[i]);
//	}
//	
//	for(i=0;i<100;i++)
//	{	
//	Uart_Send_Msg("reg_data = ");
//	Uart_Send_Hex(&reg_data[i],1);
//	Uart_Send_Msg("\r\n");
//	}
	
	
	
	
//	mdelay(1);
#endif
    FM175XX_HPD(1);  //NPD = 0	����NPDģʽ
	mdelay(10);
	FM175XX_HPD(0);  //NPD = 1
	return;
}



//***********************************************
//�������ƣ�GetReg_Ext(unsigned char ExtRegAddr,unsigned char* ExtRegData)
//�������ܣ���ȡ��չ�Ĵ���ֵ
//��ڲ�����ExtRegAddr:��չ�Ĵ�����ַ   ExtRegData:��ȡ��ֵ
//���ڲ�����unsigned char  TRUE����ȡ�ɹ�   FALSE:ʧ��
//***********************************************
void GetReg_Ext(unsigned char ExtRegAddr,unsigned char* ExtRegData)
{

	SetReg(JREG_EXT_REG_ENTRANCE,JBIT_EXT_REG_RD_ADDR + ExtRegAddr);
	GetReg(JREG_EXT_REG_ENTRANCE,&(*ExtRegData));
	return;	
}

//***********************************************
//�������ƣ�SetReg_Ext(unsigned char ExtRegAddr,unsigned char* ExtRegData)
//�������ܣ�д��չ�Ĵ���
//��ڲ�����ExtRegAddr:��չ�Ĵ�����ַ   ExtRegData:Ҫд���ֵ
//���ڲ�����unsigned char  TRUE��д�ɹ�   FALSE:дʧ��
//***********************************************
void SetReg_Ext(unsigned char ExtRegAddr,unsigned char ExtRegData)
{

	SetReg(JREG_EXT_REG_ENTRANCE,JBIT_EXT_REG_WR_ADDR + ExtRegAddr);
	SetReg(JREG_EXT_REG_ENTRANCE,JBIT_EXT_REG_WR_DATA + ExtRegData);
	return; 	
}

//*******************************************************
//�������ƣ�ModifyReg_Ext(unsigned char ExtRegAddr,unsigned char* mask,unsigned char set)
//�������ܣ��Ĵ���λ����
//��ڲ�����ExtRegAddr:Ŀ��Ĵ�����ַ   mask:Ҫ�ı��λ  
//         set:  0:��־��λ����   ����:��־��λ����
//���ڲ�����unsigned char  TRUE��д�ɹ�   FALSE:дʧ��
//********************************************************
void ModifyReg_Ext(unsigned char ExtRegAddr,unsigned char mask,unsigned char set)
{
  unsigned char regdata;
	
	GetReg_Ext(ExtRegAddr,&regdata);
	
		if(set)
		{
			regdata |= mask;
		}
		else
		{
			regdata &= ~(mask);
		}
	
	SetReg_Ext(ExtRegAddr,regdata);
	return;
}

unsigned char FM175XX_SoftReset(void)
{	
	unsigned char reg_data;
	SetReg(JREG_COMMAND,CMD_SOFT_RESET);
	mdelay(1);//FM175XXоƬ��λ��Ҫ1ms
	GetReg(JREG_COMMAND,&reg_data);
	if(reg_data == 0x20)
		return SUCCESS;
	else
		return ERROR;
}








