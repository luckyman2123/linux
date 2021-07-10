#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include "fm17750_i2c.h"
#include "FM175XX.h"

#define NFC_NETLINK_PORT		50
#define NFC_NETLINK_PORT2		51

static struct sock *nfc_nlfd = NULL;
static struct sock *nfc_nlfd2 = NULL;
extern struct i2c_client *fm_client;

extern unsigned char key_block[6];
extern unsigned char keym_block[6];
extern unsigned char write_block[16];
extern union data_value my_value;
extern bool key_auth; // 0:keyA_auth  1:keyB_auth
extern bool polling_mode; // 0:single mode  1:poling mode
extern bool have_cmd; // 0:no cmd  1:have cmd
extern unsigned char cmd;
extern unsigned char temp_sector;
extern unsigned char temp_block;
extern unsigned char cmd;

int send_nfc_msg(unsigned char *message,int msg_size)
{
    struct sk_buff *skb_out;
    struct nlmsghdr *nlh;
    int ret;
    int i = 0;

    skb_out = nlmsg_new(msg_size, GFP_ATOMIC);
    if(!skb_out)
    {
        dev_err(&fm_client->dev, "netlink_alloc_skb error");
        return -1;
    }

    nlh = nlmsg_put(skb_out, 0, 0, NETLINK_NFC, msg_size, 0);
    if(nlh == NULL)
    {
        dev_err(&fm_client->dev, "nlmsg_put() error");
        nlmsg_free(skb_out);
        return -1;
    }

    memcpy(nlmsg_data(nlh), message, msg_size);
    dev_err(&fm_client->dev, "========send_nfc_msg msg_size:%d======",msg_size);
    for(i = 0;i < msg_size;i ++)
        dev_info(&fm_client->dev, "%02X",message[i]);
    ret = netlink_unicast(nfc_nlfd, skb_out, NFC_NETLINK_PORT, MSG_DONTWAIT);

    return ret;
}
EXPORT_SYMBOL(send_nfc_msg);

int send_nfc_msg2(unsigned char *message,int msg_size)
{
    struct sk_buff *skb_out;
    struct nlmsghdr *nlh;
    int ret;
    int i = 0;

    skb_out = nlmsg_new(msg_size, GFP_ATOMIC);
    if(!skb_out)
    {
        dev_err(&fm_client->dev, "netlink_alloc_skb error");
        return -1;
    }

    nlh = nlmsg_put(skb_out, 0, 0, NETLINK_NFC2, msg_size, 0);
    if(nlh == NULL)
    {
        dev_err(&fm_client->dev, "nlmsg_put() error");
        nlmsg_free(skb_out);
        return -1;
    }

    memcpy(nlmsg_data(nlh), message, msg_size);
    dev_err(&fm_client->dev, "========send_nfc_msg msg_size:%d======",msg_size);
    for(i = 0;i < msg_size;i ++)
        dev_info(&fm_client->dev, "%02X",message[i]);
    ret = netlink_unicast(nfc_nlfd2, skb_out, NFC_NETLINK_PORT2, MSG_DONTWAIT);

    return ret;
}
EXPORT_SYMBOL(send_nfc_msg2);

void exchange_data(unsigned char *data)
{
    unsigned char temp_data = data[0];
    data[0] = data[3];
    data[3] = temp_data;
    temp_data = data[1];
    data[1] = data[2];
    data[2] = temp_data;
}

static void nfc_recv_cb(struct sk_buff *skb)
{
    struct nlmsghdr *nlh = NULL;
    unsigned char *data = NULL;
    unsigned char cmd_ack[2] = {0};
    int i = 0;
	
    if(skb->len >= nlmsg_total_size(0))
    {
        nlh = nlmsg_hdr(skb);
        data = NLMSG_DATA(nlh);
        dev_info(&fm_client->dev, "########recive app cmd = 0x%02X#########",data[0]);
        dev_info(&fm_client->dev, "len:%d, data len:%d",skb->len,skb->data_len);
        for(i = 0;i < 4;i++)
            dev_info(&fm_client->dev,"%02X %02X %02X %02X %02X %02X %02X %02X",
                data[i*8],data[i*8+1],data[i*8+2],data[i*8+3],data[i*8+4],data[i*8+5],data[i*8+6],data[i*8+7]);
        
        cmd_ack[0] = data[0];
        cmd = data[0];
        if(data[0] < CMD_START || data[0] > CMD_END)
        {
            dev_info(&fm_client->dev, "recv app cmd err!!!:type = 0x%02X",data[0]);
            cmd_ack[1] = CMD_ERROR;
            send_nfc_msg(cmd_ack,sizeof(cmd_ack));
            return;
        }
        if(data[0] >= S_NFC_BLOCKSET && data[0] <= S_NFC_BLOCKM_KEYB && polling_mode)
        {
            cmd_ack[1] = CMD_ISPOLLING;
            send_nfc_msg(cmd_ack,sizeof(cmd_ack));
            return;
        }
        else if(data[0] >= P_NFC_BLOCKSET && data[0] <= P_NFC_BLOCKM_KEYB && !polling_mode)
        {
            cmd_ack[1] = CMD_NOPOLLING;
            send_nfc_msg(cmd_ack,sizeof(cmd_ack));
            return;
        }
        switch(data[0])
        {
            case NFC_HRESET:            //0x01
                nfc_hreset_handle();
                break;
            case NFC_SRESET:            //0x02
                nfc_sreset_handle();
                break;
            case CHECK_NFC_POLLING:     //0x03
                check_nfc_polling_handle();
                break;
            case S_NFC_BLOCKSET:        //0x04
            case P_NFC_BLOCKSET:        //0x0E
                if(data[1] > 1 || data[8] > 15 || data[9] > 2)
                {
                    cmd_ack[1] = CMD_DATAERR;
                    send_nfc_msg(cmd_ack,sizeof(cmd_ack));
                    return;
                }
                key_auth = data[1];
                memcpy(key_block,(data+2),6);
                temp_sector = data[8];
                temp_block = data[9];
                memcpy(my_value.set_block,(data+10),4);
                cmd = data[0];
                have_cmd = 1;
                //s_polling_func();
                break;
            case S_NFC_BLOCKREAD:       //0x05
            case P_NFC_BLOCKREAD:       //0x0F
                if(data[1] > 1 || data[8] > 15 || data[9] > 3)
                {
                    cmd_ack[1] = CMD_DATAERR;
                    send_nfc_msg(cmd_ack,sizeof(cmd_ack));
                    return;
                }
                key_auth = data[1];
                memcpy(key_block,(data+2),6);
                temp_sector = data[8];
                temp_block = data[9];
                cmd = data[0];
                have_cmd = 1;
                //s_polling_func();
                break;
            case S_NFC_BLOCKWRITE:      //0x06
            case P_NFC_BLOCKWRITE:      //0x10
                if(data[1] > 1 || data[8] > 15 || data[9] > 2)
                {
                    cmd_ack[1] = CMD_DATAERR;
                    send_nfc_msg(cmd_ack,sizeof(cmd_ack));
                    return;
                }
                key_auth = data[1];
                memcpy(key_block,(data+2),6);
                temp_sector = data[8];
                temp_block = data[9];
                memcpy(write_block,(data+10),16);
                cmd = data[0];
                have_cmd = 1;
                //s_polling_func();
                break;
            case S_NFC_BLOCINC:         //0x07
            case P_NFC_BLOCINC:         //0x11
                if(data[1] > 1 || data[8] > 15 || data[9] > 2)
                {
                    cmd_ack[1] = CMD_DATAERR;
                    send_nfc_msg(cmd_ack,sizeof(cmd_ack));
                    return;
                }
                key_auth = data[1];
                memcpy(key_block,(data+2),6);
                temp_sector = data[8];
                temp_block = data[9];
                memcpy(my_value.inc_block,(data+10),4);
                exchange_data(my_value.inc_block);
                dev_info(&fm_client->dev, "inc: %02X %02X %02X %02X",my_value.inc_block[0],my_value.inc_block[1],my_value.inc_block[2],my_value.inc_block[3]);
                cmd = data[0];
                have_cmd = 1;
                //s_polling_func();
                break;
            case S_NFC_BLOCKDEC:        //0x08
            case P_NFC_BLOCKDEC:        //0x12
                if(data[1] > 1 || data[8] > 15 || data[9] > 2)
                {
                    cmd_ack[1] = CMD_DATAERR;
                    send_nfc_msg(cmd_ack,sizeof(cmd_ack));
                    return;
                }
                key_auth = data[1];
                memcpy(key_block,(data+2),6);
                temp_sector = data[8];
                temp_block = data[9];
                memcpy(my_value.dec_block,(data+10),4);
                exchange_data(my_value.dec_block);
                cmd = data[0];
                have_cmd = 1;
                //s_polling_func();
                break;
            case S_NFC_BLOCKRESTORE:    //0x09
            case P_NFC_BLOCKRESTORE:    //0x13
                if(data[1] > 1 || data[8] > 15 || data[9] > 3)
                {
                    cmd_ack[1] = CMD_DATAERR;
                    send_nfc_msg(cmd_ack,sizeof(cmd_ack));
                    return;
                }
                key_auth = data[1];
                memcpy(key_block,(data+2),6);
                temp_sector = data[8];
                temp_block = data[9];
                cmd = data[0];
                have_cmd = 1;
                //s_polling_func();
                break;
            case S_NFC_BLOCKM_KEYA:     //0x0A
            case P_NFC_BLOCKM_KEYA:     //0x14
                if(data[1] > 1 || data[8] > 15)
                {
                    cmd_ack[1] = CMD_DATAERR;
                    send_nfc_msg(cmd_ack,sizeof(cmd_ack));
                    return;
                }
                key_auth = data[1];
                memcpy(key_block,(data+2),6);
                temp_sector = data[8];
                memcpy(keym_block,(data+9),6);
                cmd = data[0];
                have_cmd = 1;
                //s_polling_func();
                break;
            case S_NFC_BLOCKM_KEYB:     //0x0B
            case P_NFC_BLOCKM_KEYB:     //0x15
                if(data[1] > 1 || data[8] > 15)
                {
                    cmd_ack[1] = CMD_DATAERR;
                    send_nfc_msg(cmd_ack,sizeof(cmd_ack));
                    return;
                }
                key_auth = data[1];
                memcpy(key_block,(data+2),6);
                temp_sector = data[8];
                memcpy(keym_block,(data+9),6);
                cmd = data[0];
                have_cmd = 1;
                //s_polling_func();
                break;
            case POLLING_NFC_START:     //0x0C
                polling_nfc_start_handle();
                break;
            case POLLING_NFC_STOP:      //0x0D
                polling_nfc_stop_handle();
                break;
        }
        if(data[0] >= S_NFC_BLOCKSET && data[0] <= S_NFC_BLOCKM_KEYB)
            s_polling_func();
    }
}

struct netlink_kernel_cfg nfcnl_cfg = 
{
    .groups	= 1,
    .input = nfc_recv_cb,
};

struct netlink_kernel_cfg nfcnl_cfg2 = 
{
    .groups	= 1,
    .input = NULL,
};


extern int nfc_probe_status;
static int __init nfc_netlink_init(void)
{
	if(nfc_probe_status || !fm_client)
		return -1;
    nfc_nlfd = netlink_kernel_create(&init_net, NETLINK_NFC, &nfcnl_cfg);
    if(!nfc_nlfd)
    {
        printk(KERN_ERR "Cannot create a nfc_netlink socket\n");
        return -1;
    }
	
	nfc_nlfd2 = netlink_kernel_create(&init_net, NETLINK_NFC2, &nfcnl_cfg2);
    if(!nfc_nlfd2)
    {
        printk(KERN_ERR "Cannot create a nfc_netlink2 socket\n");
        return -1;
    }

    return 0;
}

static void __exit nfc_netlink_exit(void)
{
	if(nfc_probe_status || !fm_client)
		return;
    sock_release(nfc_nlfd->sk_socket);
	sock_release(nfc_nlfd2->sk_socket);
    printk(KERN_DEBUG "netlink exit\n!");
}

module_init(nfc_netlink_init);
module_exit(nfc_netlink_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxw");
MODULE_DESCRIPTION("netlink_nfc");
