#ifndef __MG_NETLINK__
#define __MG_NETLINK__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>

#pragma pack(1)

typedef enum{
	CMD_NFC		= 0x00,
	CMD_GPS		= 0x01,
	CMD_END,
}cmd_type; // 0 for cmd type

//1 2 for len

typedef enum{
	NFC_WRITE		= 0x00,
	NFC_READ		= 0x01,
	NFC_POLLING_ON	= 0x02,
	NFC_POLLING_OFF	= 0x03,
	NFC_POLLING_CHK	= 0x04,
	NFC_HARD_RESET	= 0x05,
	NFC_SOFT_RESET	= 0x06,
	NFC_REPORT		= 0x07,
	NFC_REG_READ	= 0x08,
	NFC_REG_WRITE	= 0x09,
	NFC_APDU_START	= 0x0A,
	NFC_APDU_STOP	= 0x0B,
	S_NFC_BLOCKREAD	    = 0x0C,     //块的读
	S_NFC_BLOCKWRITE	= 0x0D,      //块的写
	S_NFC_BLOCKDEC	    = 0x0E,     //块的减值
	NFC_KARDA_REPORT	= 0x0F,
	NFC_KARDB_REPORT	= 0x10,

	NFC_END,
}nfc_cmd_type; //3 nfc cmd

typedef enum{
	GPS_WRITE		= 0x00,
	GPS_READ		= 0x01,
	GPS_POLLING_ON	= 0x02,
	GPS_POLLING_OFF	= 0x03,
	GPS_POLLING_CHK	= 0x04,
	GPS_REPORT		= 0x05,
	GPS_END,
}gps_cmd_type; //3 gps cmd

typedef enum{
	NETLINK_SUCCESS	= 0x00,
	NETLINK_FAILED	= 0x01,
	NETLINK_CMDERR	= 0x02,
	NETLINK_NOPOLL	= 0x03,
	NETLINK_NOCARD	= 0x04,
	NETLINK_NODRIV	= 0x06,//no driver
	NETLINK_AUTHENERROR =0x07,
	NETLINK_TODO	= 0xFF,//表示待实现
}NETLINK_RTN; //4 return status


/*
IIC_NL_NCPU_REQ 注释

key_type 密钥类型
key[6];  密钥数据
sector_id   那个扇区 最大值是15
block_id  那个块0,1,2
block_buf  要写进去的块数据 16个byte 

or block_buf 填入你需要减去的值(仅减值操作有效)
*/





typedef struct {
	unsigned char key_type;
	unsigned char key[6];
	unsigned char sector_id;
	unsigned char block_id;
	unsigned char block_buf[16];
}IIC_NL_NCPU_REQ, * PIIC_NL_NCPU_REQ;




typedef struct {
	unsigned char type;
	int16_t len;
	unsigned char cmd;

	union{
	IIC_NL_NCPU_REQ ncpu;
	unsigned char data[256];
	};
}IIC_NL_REQ, * PIIC_NL_REQ;




/*
IIC_NL_RESP   data[1500]定义注释
data[0-15] 读到块数据
*/
typedef struct {
	unsigned char type;
	int16_t len;
	unsigned char cmd;
	unsigned char status;
	unsigned char data[1500];
}IIC_NL_RESP, * PIIC_NL_RESP;


typedef struct {
 unsigned char type;
 int16_t len;
 unsigned char cmd;
}IIC_GPS_REQ, * PIIC_GSP_REQ;

typedef struct {
	 unsigned char type;
	 int16_t len;
	 unsigned char cmd; 
	 unsigned char status;	 
	 unsigned char data[1500];
 
}IIC_GPS_RESP, * PIIC_GSP_RESP;




int send_mg_msg(unsigned char *message,int msg_size);
int send_mg_msg2(unsigned char *message,int msg_size);

typedef void (*NETLINK_CMD_HANDLE)(unsigned char *msg);

void netlink_nfc_cmd_handle(unsigned char *msg);
void netlink_gps_cmd_handle(unsigned char *msg);

#endif
