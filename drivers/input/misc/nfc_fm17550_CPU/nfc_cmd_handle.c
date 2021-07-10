#include "nfc_cmd_handle.h"
#include "fm175xx.h"
#include "fm17750_i2c.h"
#include "MIFARE.h"
#include "READER_API.h"

#include "type_a.h"



static unsigned char key_block[6];
static unsigned char keym_block[6];
static unsigned char write_block[16];
static union data_value my_value;
static bool key_auth; // 0:keyA_auth  1:keyB_auth
static unsigned char cmd;
static unsigned char temp_sector;
static unsigned char temp_block;



extern struct i2c_client *fm_client;
extern struct delayed_work nfc_polling_work;

typedef void (*CMD_HANDLE)(void);

CMD_HANDLE cmd_handled[] = {
    NULL,
    //nfc_hreset_handle,
    //nfc_sreset_handle,
    //check_nfc_polling_handle,
    NULL,
    NULL,
    NULL,
    s_nfc_blockset_handle,
    s_nfc_blockinc_handle,
    s_nfc_blockrestore_handle,
    s_nfc_blockm_keya_handle,
    s_nfc_blockm_keyb_handle,
    //polling_nfc_start_handle,
    //polling_nfc_stop_handle,
    NULL,
    NULL,
    NULL,
    s_nfc_blockread_handle,
    s_nfc_blockwrite_handle,
    s_nfc_blockdec_handle,
    p_nfc_blockset_handle,
    p_nfc_blockread_handle,
    p_nfc_blockwrite_handle,
    p_nfc_blocinc_handle,
    p_nfc_blockdec_handle,
    p_nfc_blockrestore_handle,
    p_nfc_blockm_keya_handle,
    p_nfc_blockm_keyb_handle,
};








void exchange_data(unsigned char *data)
{
    unsigned char temp_data = data[0];
    data[0] = data[3];
    data[3] = temp_data;
    temp_data = data[1];
    data[1] = data[2];
    data[2] = temp_data;
}





void handle_nfc_cmd(unsigned char cmd)
{
    //(*(cmd_handle[cmd]))();
   // if(cmd > NFC_EN)// 22 cmd
     //   return;
    if(cmd_handled[cmd] != NULL)
        cmd_handled[cmd]();
    return;
}

void data_clean(void)
{
    memset(key_block,0,sizeof(key_block));
    memset(keym_block,0,sizeof(keym_block));
    memset(write_block,0,sizeof(write_block));
    memset(my_value.set_block,0,sizeof(my_value.set_block));
    memset(my_value.inc_block,0,sizeof(my_value.inc_block));
    memset(my_value.dec_block,0,sizeof(my_value.dec_block));
	//clear_card_status();
  //  polling_mode = 0;
   // have_cmd = 0;
    cmd = 0;
}
void s_polling_func(void)
{
/*
    unsigned char cmd_ack[2] = {0};
    unsigned char polling_card;
    unsigned char result = 0;
    int i = 0;
    for(i = 0;i < 4;i++)
    {
        dev_info(&fm_client->dev, "........s_polling_func: i = %d........\n",i);
        SetRf(3);
        if(FM175XX_Polling(&polling_card)== SUCCESS)
        {
            if(polling_card & 0x01)//TYPE A
            {
                if(TYPE_A_EVENT() == CMD_CARDAERR)
                {
                    cmd_ack[0] = cmd;
                    cmd_ack[1] = CMD_CARDAERR;
                    send_mg_msg(cmd_ack,sizeof(cmd_ack));
                }
                SetRf(0);
                break;
            }
        }
        SetRf(0);
        mdelay(100);
    }
    if(i >= 4)
    {
        cmd_ack[0] = cmd;
        cmd_ack[1] = CMD_NOCARDA;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
    }
    */

	;

}

void nfc_hreset_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    cmd_ack[0] = cmd;
    data_clean();
    FM175XX_HardReset();
    send_mg_msg(cmd_ack,sizeof(cmd_ack));
    cmd = 0;
    //have_cmd = 0;
}
void nfc_sreset_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    cmd_ack[0] = cmd;
    data_clean();
    if(FM175XX_SoftReset())
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
    }
    else
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
    cmd = 0;
}
void check_nfc_polling_handle(void)
{
    unsigned char cmd_ack[3] = {0};
    cmd_ack[0] = cmd;
   // cmd_ack[2] = polling_mode;
    send_mg_msg(cmd_ack,sizeof(cmd_ack));
    cmd = 0;
}
void s_nfc_blockset_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    unsigned char block = temp_sector*4 + temp_block;
    cmd_ack[0] = cmd;
    if(block == 0)
    {
        cmd_ack[1] = CMD_NOPERMISSION;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    if(Mifare_Auth(key_auth,temp_sector,key_block,Type_A.UID))
    {
        cmd_ack[1] = CMD_KEYERR;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    if(Mifare_Blockset(block,my_value.set_block))
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    send_mg_msg(cmd_ack,sizeof(cmd_ack));
}





void s_nfc_blockread_handle(PIIC_NL_REQ req,PIIC_NL_RESP resp)
{

    unsigned char read_block[16] = {0};
	int i=0;
	int16_t msglen;

	int ret=-1;

	msglen = 5;
	resp->type = CMD_NFC;
	resp->cmd = S_NFC_BLOCKREAD;




	Type_A.UID[0] =PICC_UID[0];
	Type_A.UID[1] =PICC_UID[1];
	Type_A.UID[2] =PICC_UID[2];
	Type_A.UID[3] =PICC_UID[3];
	

	key_auth = req->ncpu.key_type;
	memcpy(key_block,req->ncpu.key,6);
	temp_sector = req->ncpu.sector_id;
	temp_block = req->ncpu.block_id;


	unsigned char block = temp_sector*4 + temp_block;

	
	ret =Mifare_Auth(key_auth,temp_sector,key_block,Type_A.UID);
    if(ret != 0)
    {
    	printk("Mifare_Auth  ret=%d\n",ret);
		resp->status = NETLINK_FAILED;
        return resp;
    }



	ret =Mifare_Blockread(block,read_block);
    if(ret != 0)
    {
		printk("Mifare_Blockreadret =%d\n",ret);
		resp->status = NETLINK_FAILED;
         return resp;
    }
/* 
 printk("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
          read_block[0],read_block[1],read_block[2],read_block[3],read_block[4],read_block[5],read_block[6],read_block[7],
 read_block[8],read_block[9],read_block[10],read_block[11],read_block[12],read_block[13],read_block[14],read_block[15]);
*/
	
	

	memset(resp->data,0,sizeof(resp->data));
	resp->status=NETLINK_SUCCESS;
    msglen = msglen+16;
	resp->len = htons(msglen);
    memcpy(resp->data,read_block,16);
	
	return;
}

void s_nfc_blockwrite_handle(PIIC_NL_REQ req,PIIC_NL_RESP resp)
{
   int16_t msglen;

	msglen = 5;
	resp->type = CMD_NFC;
	resp->cmd = S_NFC_BLOCKWRITE;


	
	Type_A.UID[0] =PICC_UID[0];
	Type_A.UID[1] =PICC_UID[1];
	Type_A.UID[2] =PICC_UID[2];
	Type_A.UID[3] =PICC_UID[3];
	

	
	key_auth = req->ncpu.key_type;
	memcpy(key_block,(req->ncpu.key),6);
	temp_sector = req->ncpu.sector_id;
	temp_block = req->ncpu.block_id;
	memcpy(write_block,req->ncpu.block_buf,16);
	
	unsigned char block = temp_sector*4 + temp_block;

    if(Mifare_Auth(key_auth,temp_sector,key_block,Type_A.UID))
    {
		resp->status = NETLINK_AUTHENERROR;
        return resp;
    }
	
    if(Mifare_Blockwrite(block,write_block))
    {
  	 	resp->status = NETLINK_FAILED;
        return resp;
    }


	resp->status=NETLINK_SUCCESS;
	resp->len = htons(msglen); 

	return;
}

void s_nfc_blockinc_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    unsigned char block = temp_sector*4 + temp_block;
    cmd_ack[0] = cmd;
    if(block == 0)
    {
        cmd_ack[1] = CMD_NOPERMISSION;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    if(Mifare_Auth(key_auth,temp_sector,key_block,Type_A.UID))
    {
        cmd_ack[1] = CMD_KEYERR;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    if(Mifare_Blockinc(block,my_value.inc_block))
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    if(Mifare_Transfer(block))
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    send_mg_msg(cmd_ack,sizeof(cmd_ack));

}




void s_nfc_blockdec_handle(PIIC_NL_REQ req,PIIC_NL_RESP resp)
{
    unsigned char cmd_ack[2] = {0};
    cmd_ack[0] = cmd;
	
	
	int16_t msglen;

	msglen = 5;
	resp->type = CMD_NFC;
	resp->cmd = S_NFC_BLOCKDEC;



	key_auth = req->ncpu.key_type;
	memcpy(key_block,req->ncpu.key,6);
	temp_sector = req->ncpu.sector_id;
	temp_block = req->ncpu.block_id;
	memcpy(my_value.dec_block,req->ncpu.block_buf,4);
	unsigned char block = temp_sector*4 + temp_block;	
	exchange_data(my_value.dec_block);
		
	
	
    if(block == 0)
    {	
		resp->status = NETLINK_FAILED;
        return;
    }
    if(Mifare_Auth(key_auth,temp_sector,key_block,Type_A.UID))
    {

		resp->status = NETLINK_AUTHENERROR;
        return;
    }
    if(Mifare_Blockdec(block,my_value.dec_block))
    {

	  resp->status = NETLINK_FAILED;
       return;
    }
    if(Mifare_Transfer(block))
    {		
	    resp->status = NETLINK_FAILED;
        return;
    }

    resp->status=NETLINK_SUCCESS;
	resp->len = htons(msglen);
	
}
void s_nfc_blockrestore_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    unsigned char block = temp_sector*4 + temp_block;
    cmd_ack[0] = cmd;
    if(Mifare_Auth(key_auth,temp_sector,key_block,Type_A.UID))
    {
        cmd_ack[1] = CMD_KEYERR;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    if(Mifare_Restore(block))
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    send_mg_msg(cmd_ack,sizeof(cmd_ack));
}
void s_nfc_blockm_keya_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    unsigned char block = temp_sector*4 + 3;
    unsigned char temp_data[16] = {0};
    cmd_ack[0] = cmd;
    if(Mifare_Auth(key_auth,temp_sector,key_block,Type_A.UID))
    {
        cmd_ack[1] = CMD_KEYERR;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    if(Mifare_Blockread(block,temp_data))
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    memcpy(temp_data,keym_block,6);
    if(Mifare_Blockwrite(block,temp_data))
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    send_mg_msg(cmd_ack,sizeof(cmd_ack));
}
void s_nfc_blockm_keyb_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    unsigned char block = temp_sector*4 + 3;
    unsigned char temp_data[16] = {0};
    cmd_ack[0] = cmd;
    if(Mifare_Auth(key_auth,temp_sector,key_block,Type_A.UID))
    {
        cmd_ack[1] = CMD_KEYERR;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    if(Mifare_Blockread(block,temp_data))
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    memcpy((temp_data+10),keym_block,6);
    if(Mifare_Blockwrite(block,temp_data))
    {
        cmd_ack[1] = CMD_FAILED;
        send_mg_msg(cmd_ack,sizeof(cmd_ack));
        return;
    }
    send_mg_msg(cmd_ack,sizeof(cmd_ack));
}





void polling_nfc_start_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    cmd_ack[0] = cmd;
  //  if(!polling_mode)
  //  {
 //       polling_mode = 1;
//schedule_delayed_work(&nfc_polling_work, HZ/10);
 //   }
    send_mg_msg(cmd_ack,sizeof(cmd_ack));
}
void polling_nfc_stop_handle(void)
{
    unsigned char cmd_ack[2] = {0};
    cmd_ack[0] = cmd;
  //  polling_mode = 0;
	//clear_card_status();
    mdelay(100);
    send_mg_msg(cmd_ack,sizeof(cmd_ack));
}
void p_nfc_blockset_handle(void)
{
    s_nfc_blockset_handle();
}
void p_nfc_blockread_handle(void)
{
  //  s_nfc_blockread_handle();
}
void p_nfc_blockwrite_handle(void)
{
  //  s_nfc_blockwrite_handle();
}
void p_nfc_blocinc_handle(void)
{
    s_nfc_blockinc_handle();
}
void p_nfc_blockdec_handle(void)
{
    //s_nfc_blockdec_handle();
}
void p_nfc_blockrestore_handle(void)
{
    s_nfc_blockrestore_handle();
}
void p_nfc_blockm_keya_handle(void)
{
    s_nfc_blockm_keya_handle();
}
void p_nfc_blockm_keyb_handle(void)
{
    s_nfc_blockm_keyb_handle();
}
