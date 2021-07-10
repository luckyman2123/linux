/*************************************************************/
//2014.07.15修改版
/*************************************************************/
#include "spi.h"
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>

#if PSI_INTERFACE
unsigned char SPI_Init(void)
{
	SPDAT=0;
	SPSTAT=SPIF|WCOL;//清除SPIF与WCOL标志
	SPCTL=0xD0;//SSIG=1,SPEN=1,DORD=0,MSTR=1,CPOL=0,CPHA=0,SPR1=0,SPR0=0 (SPI_CLK=CPU_CLK/4)
	AUXR1=0x00;//PCA_P4=0,SPI_P4=0(SPI在P1 Port),S2_P4=0,GF2=0,ADRJ=0,DPS=0
	return OK;
}
/*******************************************************************************************************/
/*名称：																							   */
/*功能：SPI接口读取数据	SPI read function															   */
/*输入:																								   */
/*		N/A																							   */
/*输出：																							   */
/*		addr:	读取FM175XX内的寄存器地址[0x00~0x3f]	reg_address									   */
/*		rddata:	读取的数据							reg_data										   */
/*******************************************************************************************************/
unsigned char SPIRead(unsigned char addr)	//SPI读函数
{
 unsigned char data reg_value,send_data;
	SPI_CS=0;

   	send_data=(addr<<1)|0x80;
	SPSTAT=SPIF|WCOL;//清除SPIF与WCOL标志
	SPDAT=send_data;
	while(!(SPSTAT&SPIF));

	SPSTAT=SPIF|WCOL;//清除SPIF与WCOL标志
	SPDAT=0x00;
	while(!(SPSTAT&SPIF));
	reg_value=SPDAT;
	SPI_CS=1;
	return(reg_value);
}

void SPIRead_Sequence(unsigned char sequence_length,unsigned char addr,unsigned char *reg_value)	
//SPI连续读函数,用于READ FIFO函数
{
 unsigned char data i,send_data;
 	if (sequence_length==0)
		return;

	SPI_CS=0;

   	send_data=(addr<<1)|0x80;
	SPSTAT=SPIF|WCOL;//清除SPIF与WCOL标志
	SPDAT=send_data;
	while(!(SPSTAT&SPIF));

	for (i=0;i<sequence_length;i++)
	{
		SPSTAT=SPIF|WCOL;//清除SPIF与WCOL标志
		if (i==sequence_length-1)
			SPDAT=0x00;//最后一次读取时，地址填入0x00
		else
			SPDAT=send_data;

		while(!(SPSTAT&SPIF));
		*(reg_value+i)=SPDAT;
 	}
	SPI_CS=1;
	return;
}	
/*******************************************************************************************************/
/*名称：SPIWrite																					   */
/*功能：SPI接口写入数据	  SPI write function														   */
/*输入:																								   */
/*		add:	写入FM17XX内的寄存器地址[0x00~0x3f]	  reg_address									   */
/*		wrdata:   要写入的数据						  reg_data										   */
/*输出：																							   */
/*		N/A																							   */
/*******************************************************************************************************/
void SPIWrite(unsigned char addr,unsigned char wrdata)	//SPI写函数
{
unsigned char data send_data;
	SPI_CS=0;				   

	send_data=(addr<<1)&0x7e;
	
  	SPSTAT=SPIF|WCOL;//清除SPIF与WCOL标志
	SPDAT=send_data;
	while(!(SPSTAT&SPIF));

	SPSTAT=0xC0;
	SPDAT=wrdata;
	while(!(SPSTAT&SPIF));

	SPI_CS=1;
	return ;	
}

void SPIWrite_Sequence(unsigned char sequence_length,unsigned char addr,unsigned char *reg_value)
//SPI连续写函数,用于WRITE FIFO函数
{
unsigned char data send_data,i;
	if(sequence_length==0)
		return;

	SPI_CS=0;

	send_data=(addr<<1)&0x7e;
	
  	SPSTAT=SPIF|WCOL;//清除SPIF与WCOL标志
	SPDAT=send_data;
	while(!(SPSTAT&SPIF));

	for (i=0;i<sequence_length;i++)
	{
		SPSTAT=0xC0;
		SPDAT=*(reg_value+i);
		while(!(SPSTAT&SPIF));
	}

	SPI_CS=1;
	return ;	
}

#else

#define FM_MAX_RETRY_I2C_XFER (5)
#define FM_I2C_WRITE_DELAY_TIME (1)


extern struct i2c_client *fm_client;
static int i2c_err_cnt = 0;
/*	i2c read routine for API*/
char fm_i2c_read(struct i2c_client *client, unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};
    
    //mutex_lock(&lock_this);
	for (retry = 0; retry < FM_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg,
					ARRAY_SIZE(msg)) > 0)
			break;
		else
			usleep_range(FM_I2C_WRITE_DELAY_TIME * 1000,
			FM_I2C_WRITE_DELAY_TIME * 1000);
	}
    //mutex_unlock(&lock_this);

	if (FM_MAX_RETRY_I2C_XFER <= retry) {
		i2c_err_cnt ++;
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
}
/* i2c write routine for */
char fm_i2c_write(struct i2c_client *client, unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	unsigned char *buffer;
	int retry;
    buffer = kmalloc(len+1,GFP_KERNEL);
    buffer[0] = reg_addr;
    memcpy(buffer+1,data,len);
    //mutex_lock(&lock_this);
    struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len =len + 1,
		 .buf = buffer,
		},
	};
    
	for (retry = 0; retry < FM_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg,
					ARRAY_SIZE(msg)) > 0) {
			break;
		} else {
			usleep_range(FM_I2C_WRITE_DELAY_TIME * 1000,
			FM_I2C_WRITE_DELAY_TIME * 1000);
		}
	}
    kfree(buffer);
    //mutex_unlock(&lock_this);
	if (FM_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		i2c_err_cnt ++;
		return -EIO;
	}
	return 0;
}

int get_iic_error_cnt(void)
{
		return i2c_err_cnt;
}
void reset_iic_err_cnt(void)
{
	i2c_err_cnt = 0;
}

//static struct mutex lock_polling

unsigned char SPI_Init(void)
{
	reset_iic_err_cnt();
	
	return OK;
}

unsigned char SPIRead(unsigned char addr)	//SPI读函数
{
	unsigned char reg_value = 0;
	fm_i2c_read(fm_client,addr,&reg_value,1);
	return reg_value;
}

void SPIRead_Sequence(unsigned char sequence_length,unsigned char addr,unsigned char *reg_value)
{
	fm_i2c_read(fm_client,addr,reg_value,sequence_length);
	return;
}

void SPIWrite(unsigned char addr,unsigned char wrdata)	//SPI写函数
{
	fm_i2c_write(fm_client,addr,&wrdata,1);
	return;
}

void SPIWrite_Sequence(unsigned char sequence_length,unsigned char addr,unsigned char *reg_value)
{
	fm_i2c_write(fm_client,addr,reg_value,sequence_length);
	return;
}
#endif