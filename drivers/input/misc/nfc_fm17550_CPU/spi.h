#ifndef	SPI_H
#define SPI_H
#include "fm175xx.h"

#define PSI_INTERFACE 0
#if !PSI_INTERFACE
#include <linux/i2c.h>
char fm_i2c_read(struct i2c_client *client, unsigned char reg_addr, unsigned char *data, unsigned char len);
char fm_i2c_write(struct i2c_client *client, unsigned char reg_addr, unsigned char *data, unsigned char len);
#endif
unsigned char SPIRead(unsigned char addr);				//SPI¶Áº¯Êý
void SPIWrite(unsigned char add,unsigned char wrdata);	//SPIÐ´º¯Êý
void SPIRead_Sequence(unsigned char sequence_length,unsigned char addr,unsigned char *reg_value);
void SPIWrite_Sequence(unsigned char sequence_length,unsigned char addr,unsigned char *reg_value);
unsigned char SPI_Init(void);

int get_iic_error_cnt(void);
void reset_iic_err_cnt(void);


#endif