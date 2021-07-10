#ifndef NFC_CMD_HANDLE
#define NFC_CMD_HANDLE

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif


#include "../mg_netlink.h"




#if 0
typedef enum{
    CMD_NULL            = 0x00,
    CMD_START           = 0x01,
    NFC_HRESET          = 0x01,
    NFC_SRESET          = 0x02,
    CHECK_NFC_POLLING   = 0x03,
    /*
    S_NFC_BLOCKSET      = 0x04,
    S_NFC_BLOCKREAD     = 0x05,
    S_NFC_BLOCKWRITE    = 0x06,
    S_NFC_BLOCINC       = 0x07,
    S_NFC_BLOCKDEC      = 0x08,
    S_NFC_BLOCKRESTORE  = 0x09,
    S_NFC_BLOCKM_KEYA   = 0x0a,
    S_NFC_BLOCKM_KEYB   = 0x0b,
    */
    POLLING_NFC_START   = 0x0c,
    POLLING_NFC_STOP    = 0x0d,
	/*
    P_NFC_BLOCKSET      = 0x0e,
    P_NFC_BLOCKREAD     = 0x0f,
    P_NFC_BLOCKWRITE    = 0x10,
    P_NFC_BLOCINC       = 0x11,
    P_NFC_BLOCKDEC      = 0x12,
    P_NFC_BLOCKRESTORE  = 0x13,
    P_NFC_BLOCKM_KEYA   = 0x14,
    P_NFC_BLOCKM_KEYB   = 0x15,
         */
    CMD_EN             = 0x15,
  //  NFC_KARDA_REPORT    = 0x16,
   // NFC_KARDB_REPORT    = 0x17,
	NFC_I2C_READ		= 0x18,
	NFC_I2C_WRITE		= 0x19,
}cmd_type;


#endif




typedef enum{
    CMD_SUCESS      = 0x00,
    CMD_FAILED      = 0x01,
    CMD_ERROR       = 0x02,
    CMD_DATAERR     = 0x03,
    CMD_LENERR      = 0x04,
    CMD_NOCARDA     = 0x05,
    CMD_CARDAERR    = 0x06,
    CMD_CARDBERR    = 0x07,
    CMD_KEYERR      = 0x08,
    CMD_NOPERMISSION     = 0x09,
    CMD_ISPOLLING   = 0x0A,
    CMD_NOPOLLING   = 0x0B,
}cmd_rt_status;

union data_value{
    unsigned char set_block[4];
    unsigned char inc_block[4];
    unsigned char dec_block[4];
};

void handle_nfc_cmd(unsigned char cmd);
void s_polling_func(void);

void nfc_hreset_handle(void);
void nfc_sreset_handle(void);
void check_nfc_polling_handle(void);
void s_nfc_blockset_handle(void);
//void s_nfc_blockread_handle(void);
//void s_nfc_blockwrite_handle(void);
void s_nfc_blockinc_handle(void);
//void s_nfc_blockdec_handle(void);






void s_nfc_blockwrite_handle(PIIC_NL_REQ req,PIIC_NL_RESP resp);
void s_nfc_blockread_handle(PIIC_NL_REQ req,PIIC_NL_RESP resp);


void s_nfc_blockdec_handle(PIIC_NL_REQ req,PIIC_NL_RESP resp);








void s_nfc_blockrestore_handle(void);
void s_nfc_blockm_keya_handle(void);
void s_nfc_blockm_keyb_handle(void);
void polling_nfc_start_handle(void);
void polling_nfc_stop_handle(void);
void p_nfc_blockset_handle(void);
void p_nfc_blockread_handle(void);
void p_nfc_blockwrite_handle(void);
void p_nfc_blocinc_handle(void);
void p_nfc_blockdec_handle(void);
void p_nfc_blockrestore_handle(void);
void p_nfc_blockm_keya_handle(void);
void p_nfc_blockm_keyb_handle(void);
void nrf_i2cread_handle(unsigned char *data);
void nrf_i2cwrite_handle(unsigned char *data);
#endif


