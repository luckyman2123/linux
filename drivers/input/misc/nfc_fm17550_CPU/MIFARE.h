#ifndef _MIFARE_H_
#define _MIFARE_H_
#define KEY_NFC_A_AUTH 0
#define KEY_NFC_B_AUTH 1



extern unsigned char SECTOR,BLOCK,BLOCK_NUM;
extern unsigned char BLOCK_DATA[16];
extern unsigned char KEY_NFC_A[16][6];
extern unsigned char KEY_NFC_B[16][6];
extern void Mifare_Clear_Crypto(void);
extern unsigned char Mifare_Transfer(unsigned char block);
extern unsigned char Mifare_Restore(unsigned char block);
extern unsigned char Mifare_Blockset(unsigned char block,unsigned char *data_buff);
extern unsigned char Mifare_Blockinc(unsigned char block,unsigned char *data_buff);
extern unsigned char Mifare_Blockdec(unsigned char block,unsigned char *data_buff);
extern unsigned char Mifare_Blockwrite(unsigned char block,unsigned char *data_buff);
extern unsigned char Mifare_Blockread(unsigned char block,unsigned char *data_buff);
extern unsigned char Mifare_Auth(unsigned char mode,unsigned char sector,unsigned char *mifare_key,unsigned char *card_uid);
#endif
