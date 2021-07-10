#include "FM175XX.h"
#include <linux/string.h>
#include "FM175XX_REG.h"
#include "READER_API.h"
#include "MIFARE.h"
unsigned char SECTOR,BLOCK,BLOCK_NUM;
unsigned char BLOCK_DATA[16];
unsigned char KEY_NFC_A[16][6]=
							{{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//0
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//1
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//2
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//3
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//4
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//5
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//6
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//7
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//8
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//9
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//10
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//11
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//12
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//13
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//14
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};//15
							 
unsigned char KEY_NFC_B[16][6]=
							{{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//0
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//1
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//2
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//3
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//4
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//5
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//6
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//7
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//8
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//9
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//10
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//11
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//12
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//13
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},//14
							 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};//15
							
/*****************************************************************************************/
/*���ƣ�Mifare_Clear_Crypto																															 */
/*���ܣ�Mifare_Clear_Crypto�����֤��־																 									*/
/*���룺																																								 */
/*																																						 					*/
/*���:																																									 */
/*																																											 */
/*																																											 */
/*****************************************************************************************/							 
void Mifare_Clear_Crypto(void)
{
	ModifyReg(JREG_STATUS2,0x04,0);
	return;
}	
/*****************************************************************************************/
/*���ƣ�Mifare_Auth																		 */
/*���ܣ�Mifare_Auth��Ƭ��֤																 */
/*���룺mode����֤ģʽ��0��key A��֤��1��key B��֤����sector����֤�������ţ�0~15��		 */
/*		*mifare_key��6�ֽ���֤��Կ���飻*card_uid��4�ֽڿ�ƬUID����						 */
/*���:																					 */
/*		OK    :��֤�ɹ�																	 */
/*		ERROR :��֤ʧ��																	 */
/*****************************************************************************************/
 unsigned char Mifare_Auth(unsigned char mode,unsigned char sector,unsigned char *mifare_key,unsigned char *card_uid)
{
	unsigned char buff[12],result,reg_data;
	
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	
	SetTxCRC(1);
	SetRxCRC(1);
	if(mode == KEY_NFC_A_AUTH)
		command_stauts.pExBuf[0] = 0x60;//60 kayA��ָ֤��
	if(mode == KEY_NFC_B_AUTH)
		command_stauts.pExBuf[0] = 0x61;//61 keyB��ָ֤��
  command_stauts.pExBuf[1] = sector * 4;//��֤�����Ŀ�0��ַ
	command_stauts.pExBuf[2] = mifare_key[0];
	command_stauts.pExBuf[3] = mifare_key[1];
	command_stauts.pExBuf[4] = mifare_key[2];
	command_stauts.pExBuf[5] = mifare_key[3];
	command_stauts.pExBuf[6] = mifare_key[4];
	command_stauts.pExBuf[7] = mifare_key[5];
	command_stauts.pExBuf[8] = card_uid[0];
	command_stauts.pExBuf[9] = card_uid[1];
	command_stauts.pExBuf[10] = card_uid[2];
	command_stauts.pExBuf[11] = card_uid[3];

	command_stauts.Timeout = 5;//5ms
	command_stauts.nBytesToSend = 12;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 0;
	command_stauts.Cmd = CMD_AUTHENT;
	result = Command_Execute(&command_stauts);
	
	if (result == FM175XX_SUCCESS)
		{
		GetReg(JREG_STATUS2,&reg_data);
		if(reg_data & 0x08)//�жϼ��ܱ�־λ��ȷ����֤���
			return FM175XX_SUCCESS;
		else
			return FM175XX_AUTH_ERR;
		}
	return FM175XX_AUTH_ERR;
}
/*****************************************************************************************/
/*���ƣ�Mifare_Blockset									 */
/*���ܣ�Mifare_Blockset��Ƭ��ֵ����							 */
/*���룺block����ţ�*buff����Ҫ���õ�4�ֽ���ֵ����					 */
/*											 */
/*���:											 */
/*		OK    :���óɹ�								 */
/*		ERROR :����ʧ��								 */
/*****************************************************************************************/
 unsigned char Mifare_Blockset(unsigned char block,unsigned char *data_buff)
 {
	unsigned char block_data[16],result;
	block_data[0] = data_buff[3];
	block_data[1] = data_buff[2];
	block_data[2] = data_buff[1];
	block_data[3] = data_buff[0];
	block_data[4] = ~data_buff[3];
	block_data[5] = ~data_buff[2];
	block_data[6] = ~data_buff[1];
	block_data[7] = ~data_buff[0];
	block_data[8] = data_buff[3];
	block_data[9] = data_buff[2];
	block_data[10] = data_buff[1];
	block_data[11] = data_buff[0];
	block_data[12] = block;
	block_data[13] = ~block;
	block_data[14] = block;
	block_data[15] = ~block;
	result = Mifare_Blockwrite(block,block_data);
	return result;
 }
/*****************************************************************************************/
/*���ƣ�Mifare_Blockread																 */
/*���ܣ�Mifare_Blockread��Ƭ�������													 */
/*���룺block����ţ�0x00~0x3F����buff��16�ֽڶ�����������								 */
/*���:																					 */
/*		OK    :�ɹ�																		 */
/*		ERROR :ʧ��																		 */
/*****************************************************************************************/
unsigned char Mifare_Blockread(unsigned char block,unsigned char *data_buff)
{	
	unsigned char buff[16],result;
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	
	SetTxCRC(1);
	SetRxCRC(1);
	
	command_stauts.pExBuf[0] = 0x30;//30 ����ָ��
	command_stauts.pExBuf[1] = block;//���ַ
	command_stauts.nBytesToSend = 2;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 16;
	command_stauts.Timeout = 5;//5ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	result = Command_Execute(&command_stauts);
	if ((result != FM175XX_SUCCESS )||(command_stauts.nBytesReceived != 16)) //���յ������ݳ���Ϊ16
		return FM175XX_COMM_ERR;
	memcpy(data_buff,command_stauts.pExBuf,16);
	return FM175XX_SUCCESS;
}
/*****************************************************************************************/
/*���ƣ�mifare_blockwrite																 */
/*���ܣ�Mifare��Ƭд�����																 */
/*���룺block����ţ�0x00~0x3F����buff��16�ֽ�д����������								 */
/*���:																					 */
/*		OK    :�ɹ�																		 */
/*		ERROR :ʧ��																		 */
/*****************************************************************************************/
unsigned char Mifare_Blockwrite(unsigned char block,unsigned char *data_buff)
{	
	unsigned char buff[16];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	
	SetTxCRC(1);
	SetRxCRC(0);

	command_stauts.pExBuf[0] = 0xA0;//A0 д��ָ��
	command_stauts.pExBuf[1] = block;//���ַ

	command_stauts.nBytesToSend = 2;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 1;
	command_stauts.Timeout = 10;//10ms
	command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);
	if ((command_stauts.nBitsReceived != 4)||((command_stauts.pExBuf[0]&0x0F)!=0x0A))	//���δ���յ�0x0A����ʾ��ACK
		return FM175XX_COMM_ERR;
	
	command_stauts.Timeout = 10;//10ms
	memcpy(command_stauts.pExBuf,data_buff,16);
	command_stauts.nBytesToSend = 16;
	command_stauts.nBytesToReceive = 1;
	command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);
	if ((command_stauts.nBitsReceived != 4)||((command_stauts.pExBuf[0]&0x0F)!=0x0A))	//���δ���յ�0x0A����ʾ��ACK
		return FM175XX_COMM_ERR;
	return FM175XX_SUCCESS;
}

/*****************************************************************************************/
/*���ƣ�																				 */
/*���ܣ�Mifare ��Ƭ��ֵ����																 */
/*���룺block����ţ�0x00~0x3F����buff��4�ֽ���ֵ��������								 */
/*���:																					 */
/*		OK    :�ɹ�																		 */
/*		ERROR :ʧ��																		 */
/*****************************************************************************************/
unsigned char Mifare_Blockinc(unsigned char block,unsigned char *data_buff)
{	
	unsigned char buff[4];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	SetTxCRC(1);
	SetRxCRC(0);
	command_stauts.pExBuf[0] = 0xC1;// C1 ��ֵָ��
	command_stauts.pExBuf[1] = block;//���ַ
	
	command_stauts.nBytesToSend = 2;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 1;
  command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);
	if ((command_stauts.nBitsReceived != 4)||((command_stauts.pExBuf[0] & 0x0F) != 0x0A))	//���δ���յ�0x0A����ʾ��ACK
		return FM175XX_COMM_ERR;
	
	command_stauts.Timeout = 10;//10ms
	memcpy(command_stauts.pExBuf,data_buff,4);
	command_stauts.nBytesToSend = 4;
	command_stauts.nBytesToReceive = 0;
	command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);

	return FM175XX_SUCCESS;
}
/*****************************************************************************************/
/*���ƣ�mifare_blockdec																	 */
/*���ܣ�Mifare ��Ƭ��ֵ����																 */
/*���룺block����ţ�0x00~0x3F����buff��4�ֽڼ�ֵ��������								 */
/*���:																					 */
/*		OK    :�ɹ�																		 */
/*		ERROR :ʧ��																		 */
/*****************************************************************************************/
unsigned char Mifare_Blockdec(unsigned char block,unsigned char *data_buff)
{	
	unsigned char buff[4];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	
	SetTxCRC(1);
	SetRxCRC(0);
	command_stauts.pExBuf[0] = 0xC0;// C0 ��ֵָ��
	command_stauts.pExBuf[1] = block;//���ַ
	
	command_stauts.nBytesToSend = 2;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 1;
  command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);
	if ((command_stauts.nBitsReceived != 4)||((command_stauts.pExBuf[0]&0x0F)!=0x0A))	//���δ���յ�0x0A����ʾ��ACK
		return FM175XX_COMM_ERR;
	
	command_stauts.Timeout = 10;//10ms
	memcpy(command_stauts.pExBuf,data_buff,4);
	command_stauts.nBytesToSend = 4;
	command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);

	return FM175XX_SUCCESS;
}
/*****************************************************************************************/
/*���ƣ�mifare_transfer																	 */
/*���ܣ�Mifare ��Ƭtransfer����															 */
/*���룺block����ţ�0x00~0x3F��														 */
/*���:																					 */
/*		OK    :�ɹ�																		 */
/*		ERROR :ʧ��																		 */
/*****************************************************************************************/
unsigned char Mifare_Transfer(unsigned char block)
{	
	unsigned char buff[4];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	
	SetTxCRC(1);
	SetRxCRC(0);
	command_stauts.pExBuf[0] = 0xB0;// B0 Transferָ��
	command_stauts.pExBuf[1] = block;//���ַ
	
	command_stauts.nBytesToSend = 2;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 1;
  command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);
	if ((command_stauts.nBitsReceived != 4)||((command_stauts.pExBuf[0]&0x0F)!=0x0A))	//���δ���յ�0x0A����ʾ��ACK
		return FM175XX_COMM_ERR;
	return FM175XX_SUCCESS;
}
/*****************************************************************************************/
/*���ƣ�mifare_restore																	 */
/*���ܣ�Mifare ��Ƭrestore����															 */
/*���룺block����ţ�0x00~0x3F��														 */
/*���:																					 */
/*		OK    :�ɹ�																		 */
/*		ERROR :ʧ��																		 */
/*****************************************************************************************/

unsigned char Mifare_Restore(unsigned char block)
{	
	unsigned char buff[4];
	command_struct command_stauts;
	command_stauts.pExBuf = buff;
	
	SetTxCRC(1);
	SetRxCRC(0);
	command_stauts.pExBuf[0] = 0xC2;// C2 Restoreָ��
	command_stauts.pExBuf[1] = block;//���ַ
	
	command_stauts.nBytesToSend = 2;
	command_stauts.nBitsToSend = 0;
	command_stauts.nBytesToReceive = 1;
	command_stauts.Timeout = 10;//10ms
  command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);
	if ((command_stauts.nBitsReceived != 4)||((command_stauts.pExBuf[0]&0x0F)!=0x0A))	//���δ���յ�0x0A����ʾ��ACK
		return FM175XX_COMM_ERR;
	
	command_stauts.Timeout = 10;//10ms
	memcpy(command_stauts.pExBuf,"\x00\x00\x00\x00",4);
	command_stauts.nBytesToSend = 4;
	command_stauts.nBytesToReceive = 1;
	command_stauts.Cmd = CMD_TRANSCEIVE;
	Command_Execute(&command_stauts);

	return FM175XX_SUCCESS;
}
