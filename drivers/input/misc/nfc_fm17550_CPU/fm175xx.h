#ifndef FM175X_H
#define FM175X_H

#define TIMEOUT_Err	(0x20)
#define ERROR		(1)
#define	OK			(0)

#define CommandReg	0x01
#define ComIEnReg	0x02
#define DivIEnReg	0x03
#define ComIrqReg	0x04
#define DivIrqReg	0x05
#define ErrorReg	0x06
#define Status1Reg	0x07
#define Status2Reg	0x08
#define FIFODataReg 0x09
#define FIFOLevelReg 0x0A
#define WaterLevelReg 0x0B
#define ControlReg	0x0C
#define BitFramingReg 0x0D
#define CollReg	0x0E
#define ModeReg 0x11
#define TxModeReg 0x12
#define RxModeReg 0x13
#define TxControlReg 0x14
#define TxAutoReg 0x15
#define TxSelReg 0x16
#define RxSelReg 0x17
#define RxThresholdReg 0x18
#define DemodReg 0x19
#define MfTxReg 0x1C
#define MfRxReg 0x1D
#define SerialSpeedReg 0x1F
#define CRCMSBReg 0x21
#define CRCLSBReg 0x22
#define ModWidthReg 0x24
#define GsNOffReg 0x23
#define TxBitPhaseReg 0x25
#define RFCfgReg 0x26
#define GsNOnReg 0x27
#define CWGsPReg 0x28
#define ModGsPReg 0x29
#define TModeReg 0x2A
#define TPrescalerReg 0x2B
#define TReloadMSBReg 0x2C
#define TReloadLSBReg 0x2D
#define TCounterValMSBReg 0x2E
#define TCounterValLSBReg 0x2F
#define TestSel1Reg 0x31
#define TestSel2Reg 0x32
#define TestPinEnReg 0x33
#define TestPinValueReg 0x34
#define TestBusReg 0x35
#define AutoTestReg 0x36
#define VersionReg 0x37
#define AnalogTestReg 0x38
#define TestDAC1Reg 0x39
#define TestDAC2Reg 0x3A
#define TestADCReg 0x3B

#define Idle	(0x00 | 0x20)
#define Mem		0x01
#define RandomID	0x02
#define CalcCRC	0x03
#define Transmit	0x04
#define NoCmdChange	0x07
#define Receive	0x08
#define Transceive	0x0C
#define MFAuthent	0x0E
#define SoftReset	0x0F

//发射参数设置
#define MODWIDTH 0x26 //默认设置为 0x26 ，0x26 = 106K，0x13 = 212K， 0x09 = 424K ，0x04 = 848K

//接收参数配置
#define RXGAIN	6		//设置范围0~7
#define GSNON	15			//设置范围0~15
#define MODGSNON 8 	//设置范围0~15
#define GSP 31			//设置范围0~63
#define MODGSP 31		//设置范围0~63
#define COLLLEVEL 4	//设置范围0~7
#define MINLEVEL  8	//设置范围0~15
#define RXWAIT 4		//设置范围0~63
#define UARTSEL 2		//默认设置为2  设置范围0~3 0:固定低电平 1:TIN包络信号 2:内部接收信号 3:TIN调制信号

#define BIT0               0x01
#define BIT1               0x02
#define BIT2               0x04
#define BIT3               0x08
#define BIT4               0x10
#define BIT5               0x20
#define BIT6               0x40
#define BIT7               0x80
typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;

//---------------------------------

unsigned char Pcd_Comm( unsigned char Command, 
		               unsigned char *pInData, 
		               unsigned char InLenByte,
		               unsigned char *pOutData, 
		               unsigned int *pOutLenBit);
unsigned char Set_Rf(unsigned char mode);
unsigned char Read_Reg(unsigned char reg_add);
unsigned char Read_Reg_All(unsigned char *reg_value);
unsigned char Write_Reg(unsigned char reg_add,unsigned char reg_value);
void ModifyReg(unsigned char addr,unsigned char mask,unsigned char set);

void Write_FIFO(unsigned char length,unsigned char *fifo_data);
void Read_FIFO(unsigned char length,unsigned char *fifo_data);
unsigned char Clear_FIFO(void);
unsigned char Set_BitMask(unsigned char reg_add,unsigned char mask);
unsigned char Clear_BitMask(unsigned char reg_add,unsigned char mask);
unsigned char Pcd_ConfigISOType(unsigned char type);
unsigned char FM175XX_SoftReset(void);
unsigned char FM175XX_HardReset(void);
unsigned char FM175XX_SoftPowerdown(void);
unsigned char FM175XX_HardPowerdown(unsigned char mode);
unsigned char Pcd_SetTimer(unsigned int delaytime);
unsigned char Read_Ext_Reg(unsigned char reg_add);
unsigned char Write_Ext_Reg(unsigned char reg_add,unsigned char reg_value);
unsigned char Set_Ext_BitMask(unsigned char reg_add,unsigned char mask);
extern void GetReg(unsigned char addr,unsigned char *regdata);
extern void SetReg(unsigned char addr,unsigned char regdata);
extern void SPI_Write_FIFO(unsigned char reglen,unsigned char* regbuf);
extern void SPI_Read_FIFO(unsigned char reglen,unsigned char* regbuf);


unsigned char Clear_Ext_BitMask(unsigned char reg_add,unsigned char mask);
#endif