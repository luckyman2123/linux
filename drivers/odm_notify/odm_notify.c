#include <linux/module.h>   
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/printk.h>
#include <net/sock.h>
#include <odm-msg/msg.h>


struct sock *nl_sk = NULL;
unsigned int pid_user=0;
rwlock_t g_lock;
void odm_notify(odm_msg_type *msg, unsigned int len)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int rc;
	int sz = NLMSG_SPACE(len);
	if(!msg)
    {
        pr_info("%s:msg point NULL!\n", __func__);
		return ;
    }
	if(!nl_sk)
		return ;
	if(pid_user == 0)
		return ;
	
	skb = alloc_skb(sz, GFP_ATOMIC);
	if (!skb) {
		pr_err("%s:alloc skb failed.\n", __func__);
		return;
	}
	
	nlh = nlmsg_put(skb, 0, 0, 0, len, 0);
	NETLINK_CB(skb).portid = 0;  /* from kernel */

	memcpy(NLMSG_DATA(nlh), msg, len);

	rc = netlink_unicast(nl_sk, skb, pid_user, MSG_DONTWAIT);
	if (rc < 0) 
	{
		pr_err("%s:can not unicast skb (%d)\n", __func__, rc);
		return ;
	}
}
EXPORT_SYMBOL(odm_notify);
void nl_data_ready (struct sk_buff *__skb)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	
	skb = skb_get(__skb);
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		write_lock_bh(&g_lock);
		pid_user = nlh->nlmsg_pid;       /*pid of sending process */
		write_unlock_bh(&g_lock);
		kfree_skb(skb);
	}
	return ;
}

struct netlink_kernel_cfg cfg = {
    .groups	= 1,
    .input	= nl_data_ready,
};

static int __init odm_notify_init(void) 
{
    pr_info("%s:netlink_init.\n", __func__);
    rwlock_init(&g_lock);

	nl_sk = netlink_kernel_create(&init_net, NETLINK_ODM_MSG, &cfg);
	if (nl_sk == NULL) 
	{
		pr_err("%s:Cannot create netlink socket.\n", __func__);
		pid_user=0;
		return -ENOMEM;
	}
    pr_info("%s:creat socket ok.\n", __func__);
	return 0;
}

static void __exit odm_notify_exit(void)
{
	if (nl_sk != NULL){
    	sock_release(nl_sk->sk_socket);
  	}	
    pr_info("%s:release socket ok.\n", __func__);
}
module_init(odm_notify_init);
module_exit(odm_notify_exit);
module_param(pid_user, int, 0444);
MODULE_AUTHOR("songchenglin");
