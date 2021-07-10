#include <linux/init.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include "mg_netlink.h"

#define MG_NETLINK_PORT		52
#define MG_NETLINK_PORT2		53

static struct sock *mg_nlfd = NULL;
static struct sock *mg_nlfd2 = NULL;

NETLINK_CMD_HANDLE cmd_handle[] = {
	netlink_nfc_cmd_handle,
	netlink_gps_cmd_handle,
};

int send_mg_msg(unsigned char *message,int msg_size)
{
    struct sk_buff *skb_out;
    struct nlmsghdr *nlh;
    int ret;
    int i = 0;

    skb_out = nlmsg_new(msg_size, GFP_ATOMIC);
    if(!skb_out)
    {
        pr_info("netlink_alloc_skb error");
        return -1;
    }

    nlh = nlmsg_put(skb_out, 0, 0, NETLINK_MG, msg_size, 0);
    if(nlh == NULL)
    {
        pr_info("nlmsg_put() error");
        nlmsg_free(skb_out);
        return -1;
    }

    memcpy(nlmsg_data(nlh), message, msg_size);
    //pr_info("========send_mg_msg msg_size:%d======",msg_size);
    //for(i = 0;i < msg_size;i ++)
    //    pr_info("%02X",message[i]);
    ret = netlink_unicast(mg_nlfd, skb_out, MG_NETLINK_PORT, MSG_DONTWAIT);

    return ret;
}
EXPORT_SYMBOL(send_mg_msg);

int send_mg_msg2(unsigned char *message,int msg_size)
{
    struct sk_buff *skb_out;
    struct nlmsghdr *nlh;
    int ret;
    int i = 0;

    skb_out = nlmsg_new(msg_size, GFP_ATOMIC);
    if(!skb_out)
    {
        pr_info("netlink_alloc_skb error");
        return -1;
    }

    nlh = nlmsg_put(skb_out, 0, 0, NETLINK_MG2, msg_size, 0);
    if(nlh == NULL)
    {
        pr_info("nlmsg_put() error");
        nlmsg_free(skb_out);
        return -1;
    }

    memcpy(nlmsg_data(nlh), message, msg_size);
    //pr_info("========send_mg_msg msg_size:%d======",msg_size);
    //for(i = 0;i < msg_size;i ++)
    //    pr_info("%02X",message[i]);
    ret = netlink_unicast(mg_nlfd2, skb_out, MG_NETLINK_PORT2, MSG_DONTWAIT);

    return ret;
}
EXPORT_SYMBOL(send_mg_msg2);

static void mg_recv_cb(struct sk_buff *skb)
{
    struct nlmsghdr *nlh = NULL;
    unsigned char *data = NULL;
    unsigned char cmd_ack[5] = {0};
	
    if(skb->len >= nlmsg_total_size(0))
    {
        nlh = nlmsg_hdr(skb);
        data = NLMSG_DATA(nlh);
        pr_info("mg_netlink-> cmd_type = 0x%02X,len = 0x%02X%02X\n",data[0],data[1],data[2]);
        if(data[0] >= CMD_END)
		{
			cmd_ack[0] = data[0];
			cmd_ack[1] = 0;
			cmd_ack[2] = 5;
			cmd_ack[3] = data[3];
			cmd_ack[4] = NETLINK_CMDERR;
			send_mg_msg(cmd_ack,cmd_ack[2]);
		}
		else
		{
			cmd_handle[data[0]](data);
		}
	}
}

struct netlink_kernel_cfg mgnl_cfg = 
{
    .groups	= 1,
    .input = mg_recv_cb,
};

struct netlink_kernel_cfg mgnl_cfg2 = 
{
    .groups	= 1,
    .input = NULL,
};


static int __init mg_netlink_init(void)
{
    mg_nlfd = netlink_kernel_create(&init_net, NETLINK_MG, &mgnl_cfg);
    if(!mg_nlfd)
    {
        printk(KERN_ERR "Cannot create a mg_netlink socket\n");
        return -1;
    }
	
	mg_nlfd2 = netlink_kernel_create(&init_net, NETLINK_MG2, &mgnl_cfg2);
    if(!mg_nlfd2)
    {
		sock_release(mg_nlfd->sk_socket);
        printk(KERN_ERR "Cannot create a mg_netlink2 socket\n");
        return -1;
    }

    return 0;
}

static void __exit mg_netlink_exit(void)
{
    sock_release(mg_nlfd->sk_socket);
	sock_release(mg_nlfd2->sk_socket);
    printk(KERN_DEBUG "netlink exit\n!");
}

module_init(mg_netlink_init);
module_exit(mg_netlink_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxw");
MODULE_DESCRIPTION("mg_netlink");
