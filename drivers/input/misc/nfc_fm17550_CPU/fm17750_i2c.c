/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename fm17550_i2c.c
 * @date     2014/11/25 14:40
 * @id       "20f77db"
 * @version  1.3
 *
 * @brief
 * This file implements moudle function, which add
 * the driver to I2C core.
*/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/wakelock.h>
#include "spi.h"
#include <linux/regulator/consumer.h>
#include "fm17750_i2c.h"
#include "type_a.h"
#include "cpu_app.h"
#include "spi.h"
#include "../mg_netlink.h"

#include "nfc_cmd_handle.h"

#include "MIFARE.h"













/*! @defgroup fm17550_i2c_src
 *  @brief fm17550 i2c driver module
 @{*/

#define FM_MAX_RETRY_I2C_XFER (10)
#define FM_I2C_WRITE_DELAY_TIME (1)

struct i2c_client *fm_client;
int nfc_reset_gpio = 0;

static struct i2c_driver fm_i2c_driver;

struct fm_client_data *pdata = NULL;
static bool  polling_mode = 0;                 
struct delayed_work nfc_polling_work;
//unsigned char *netlink_data;
unsigned char netlink_data[256];
static bool netlink_cmd = 0;
static bool nfc_find = 0;






static bool nfc_probe = 0;

struct wake_lock nfc_opt_lock;
int nfc_apdu = 0;













void nfc_handle(PIIC_NL_REQ req)
{
	IIC_NL_RESP resp;
	int16_t msglen;
	int rtn,reqlen;
	

	//pr_info("%d/%s :entry\n",__LINE__,__FUNCTION__);
	
	memset(&resp,0,sizeof(resp));
	
	msglen = 5;
	resp.type = CMD_NFC;
	resp.cmd = req->cmd;
	reqlen = ntohs(req->len);
	reqlen = reqlen - 4;
	if(!nfc_probe)
	{
		resp.status = NETLINK_NODRIV;
		goto EXIT_OK;
	}	
	//pr_info("%d/%s :entry reqlen:%d\n",__LINE__,__FUNCTION__,reqlen);
	if (req->cmd == NFC_WRITE) {
		pr_info("%d/%s :entry\n",__LINE__,__FUNCTION__);
		if (reqlen < 1) {
			resp.status = NETLINK_FAILED;
			goto EXIT_OK;
		}
		if(! nfc_find)
		{
			resp.status = NETLINK_NOCARD;
			goto EXIT_OK;
		}
		rtn = call_apdu_cmd(req->data,reqlen,resp.data);
		pr_info("%d/%s call_apdu_cmd:rtn:%d\n",__LINE__,__FUNCTION__,rtn);
		
		msglen += rtn;
	}else if(req->cmd == NFC_POLLING_ON) {
		if(FM175XX_HardReset()) {
			resp.status =  NETLINK_FAILED;
			goto EXIT_OK;
		} else{
			
			Pcd_ConfigISOType(0);
			nfc_find = 0;
			
			if(!polling_mode) {
				schedule_delayed_work(&nfc_polling_work, HZ/10);
			}
			polling_mode = 1;
			resp.status = NETLINK_SUCCESS;
		}
	} else if(req->cmd == NFC_POLLING_OFF){
		Set_Rf(0);
		FM175XX_HardPowerdown(1);
		polling_mode = 0;
		nfc_find = 0;
		resp.status = NETLINK_SUCCESS;
		Set_Rf(0);
	} else if(req->cmd == NFC_POLLING_CHK)
	{
		msglen += 1;
		resp.status = NETLINK_SUCCESS;
		resp.data[0] = polling_mode;
	} else if(req->cmd == NFC_HARD_RESET )	{
		
		Set_Rf(0);
		
		resp.status = FM175XX_HardReset();
		Pcd_ConfigISOType(0);
		polling_mode = 0;
		netlink_cmd = 0;
		nfc_find = 0;
		Set_Rf(0);
	}
	else if(req->cmd == NFC_SOFT_RESET)
	{
		Set_Rf(0);
		resp.status = FM175XX_SoftReset();;
		Pcd_ConfigISOType(0);
		polling_mode = 0;
		netlink_cmd = 0;
		nfc_find = 0;
		Set_Rf(0);
	} else if(req->cmd == NFC_REG_READ){
		
		msglen += 1;
		resp.status = NETLINK_SUCCESS;
		resp.data[0] = SPIRead(req->data[0]);
		pr_info("%d/%s i2c read: r:%02x,rtn:%02x\n",__LINE__,__FUNCTION__,req->data[0],resp.data[0]);

	} else if(req->cmd == NFC_REG_WRITE){
		resp.status = NETLINK_SUCCESS;
		SPIWrite(req->data[0],req->data[1]);
		pr_info("%d/%s i2c write: w:%02x/%02x\n",__LINE__,__FUNCTION__,req->data[0],req->data[1]);
	} else if(req->cmd == NFC_APDU_START){
		resp.status = NETLINK_SUCCESS;
		nfc_apdu = 1;
		pr_info(" NFC_APDU_START\n");
	}else if(req->cmd == S_NFC_BLOCKREAD){    
		if(req->ncpu.key_type > 1 || req->ncpu.sector_id > 15 || req->ncpu.block_id >2)
		{
			resp.status = NETLINK_FAILED;
		   goto EXIT_OK;
		}

		s_nfc_blockread_handle(req,&resp);
		if(resp.status ==NETLINK_SUCCESS) 
			msglen = msglen+16;
		
	}else if(req->cmd == S_NFC_BLOCKWRITE){  
		if(req->ncpu.key_type > 1 || req->ncpu.sector_id > 15 || req->ncpu.block_id >2)
		{
			resp.status = NETLINK_FAILED;
		   goto EXIT_OK;
		}
	
		s_nfc_blockwrite_handle(req,&resp);

	}else if(req->cmd == S_NFC_BLOCKDEC){      
		if(req->ncpu.key_type > 1 || req->ncpu.sector_id > 15 || req->ncpu.block_id >2)
		{
			resp.status = NETLINK_FAILED;
		   goto EXIT_OK;	
		}

		s_nfc_blockdec_handle(req,&resp);
		
	}else if(req->cmd == NFC_APDU_STOP){
		resp.status = NETLINK_SUCCESS;
		nfc_apdu = 0;
		pr_info("NFC_APDU_STOP\n");
	}
	
EXIT_OK:

	resp.len = htons(msglen);
	send_mg_msg((unsigned char *)&resp,msglen);	
	pr_info("%d/%s msglen:rtn:%d\n",__LINE__,__FUNCTION__,msglen);
	
	return;

} 

void netlink_nfc_cmd_handle(unsigned char *msg)
{
		
	PIIC_NL_REQ req;
	
	
	wake_lock(&nfc_opt_lock);
	
	if (msg == NULL) {
		
		goto EXIT_OK; 
	}
	req = (PIIC_NL_REQ) msg;
	
	nfc_handle(req);
	
EXIT_OK:	
	
	//if (wake_lock_active(&nfc_opt_lock)) {
		wake_unlock(&nfc_opt_lock);
	//}
	return;
}

void nfc_card_states(int nfc_attached,unsigned char *uid)
{
	IIC_NL_RESP resp;
	int16_t msglen;
	
	memset(&resp,0,sizeof(resp));
	resp.type = CMD_NFC;
	resp.cmd = NFC_REPORT;
	msglen = 5;
	if (nfc_attached == 1) {
		resp.status = 0x01;
		memcpy(resp.data,uid,4);
		msglen += 4;
	} else if (nfc_attached == 2) {
		resp.status = 0x02;
		memcpy(resp.data,uid,4);
		msglen += 4;
		
	} else {
		resp.status = 0x00;
	}
	
	resp.len = htons(msglen);
	send_mg_msg2((unsigned char *)&resp,msglen);
	
	pr_info("%d/%s ,atta:%d\n",__LINE__,__FUNCTION__,nfc_attached);
}

void TYPE_A_EVENT(void)
{
	#define FIND_TIME_CNT_MAX 3
	static int find_times = FIND_TIME_CNT_MAX;
	static int pwcnt = 0, pwok = 0;
	unsigned char rest;
	
	if(!polling_mode)
		return;

	pwcnt ++;
	if (pwcnt < 160) {
		if (nfc_apdu == 1) {
			return;
		}
	}
	
	/*
		上电0.5秒后下电， 如果读取到卡，不下电
	*/

	if (pwcnt == 4 ) {
		
		if (get_iic_error_cnt() > 16) {
			//错误码超过16，进行硬件复位
			reset_iic_err_cnt();
			pr_info("FM175XX_HardReset ---- \n");
			
			if(FM175XX_HardReset()) {
				/*
					硬件复位失败，下次进入继续reset
				*/
				pwcnt = 0;
				return;
			}
			Pcd_ConfigISOType(0);			
		}
		
		FM175XX_Initial_ReaderA();
		Set_Rf(3);
		pwok = 1;
	} else if (pwcnt > 15 ) {
		if(0 == nfc_find) {
			Set_Rf(0);
			pwcnt = 0;
			pwok = 0;
		}
	} else if (pwcnt > 160 ) {
		//如果计数超过160，强制关闭RF
		Set_Rf(0);
		pwcnt = 0;
		pwok = 0;
		nfc_find = 0;
		return;
	}
	
	if (pwok == 0) {
		return;
	}
	//上电500毫秒才寻卡
	if (pwcnt <= 10) {
		return;
	}
	
	rest = TypeA_CardActivate(PICC_ATQA,PICC_UID,PICC_SAK);
	//pr_info("TYPE_A_EVENT %d ------\r\n",rest);
	if(rest == OK)
	{
		if(
			(0 == nfc_find) && 
			(
				(PICC_SAK[0]&0x20) ||
				(((PICC_SAK[0]&0x08)!=0x08)&&(PICC_SAK[0]&0x20))||
				((PICC_SAK[0]&0x28)==0x28)
			)
		   )
		{
			//cpu卡
			if (call_rats() == 1) {
				nfc_find = 1;
				nfc_card_states(1,PICC_UID);
				pr_info("nfc_fm17750->find CPU NFC card:%02X %02X %02X %02X\n",PICC_UID[0],PICC_UID[1],PICC_UID[2],PICC_UID[3]);
			}
				
		} else if (PICC_SAK[0]==0x08){
			//非cpu卡
			nfc_find = 1;
			nfc_card_states(2,PICC_UID);
			pr_info("nfc_fm17750->find NCPU NFC card:%02X %02X %02X %02X\n",PICC_UID[0],PICC_UID[1],PICC_UID[2],PICC_UID[3]);
		}
		
		reset_iic_err_cnt();
		
		find_times = FIND_TIME_CNT_MAX;
	}else
	{
		if(1 == nfc_find)
		{
			find_times --;
			if(0 == find_times)
			{

				find_times = FIND_TIME_CNT_MAX;
				
				nfc_card_states(0,NULL);
				pr_info("nfc_fm17750->NFC card off 1.6\n");
				nfc_find = 0;
			}
		}
		
	}
	//Set_Rf(0);
	
	return;
}

static void polling_nfc_func(struct work_struct *work)
{
	
	if(!polling_mode) {
		return;
	}

	//if (!wake_lock_active(&nfc_opt_lock)) {
		wake_lock(&nfc_opt_lock);
	//}
		
	TYPE_A_EVENT();
	
	//if (wake_lock_active(&nfc_opt_lock)) {
		wake_unlock(&nfc_opt_lock);
	//}
	
	schedule_delayed_work(&nfc_polling_work, HZ/10);
}

static ssize_t show_driver_attr_nfc_polling(struct device_driver *ddri, const char *buf, size_t count)
{
	return snprintf(buf, 20, "%d\n", polling_mode);
}
static ssize_t store_driver_attr_nfc_polling(struct device_driver *ddri, const char *buf, size_t count)
{
	if(buf[0] == '0')
	{
		//Set_Rf(0);
		polling_mode = 0;
	}
	else
	{
		//Set_Rf(3);
		//Pcd_ConfigISOType(0);
		schedule_delayed_work(&nfc_polling_work, HZ/10);
		polling_mode = 1;
	}
	
	return count;
}
static DRIVER_ATTR(nfc_polling,	S_IWUSR | S_IRUGO, show_driver_attr_nfc_polling, store_driver_attr_nfc_polling);

//////probe/////////
int nfc_probe_status = 0;
static int fm_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	struct fm_client_data *client_data = NULL;
    u8 data;
	//////////////////////////////nrf reset gpio////////////////////////////////////////

	enum of_gpio_flags flags;
	
	pr_info("fm_i2c_probe  spin_lock_init 1");
	wake_lock_init(&nfc_opt_lock, WAKE_LOCK_SUSPEND, "nfc opt lock");
	pr_info("fm_i2c_probe  spin_lock_init 2");
	
	//nfc_reset_gpio = of_get_gpio_flags(client->dev.of_node, 0, &flags);
    nfc_reset_gpio = of_get_named_gpio_flags(client->dev.of_node,"nfc_reset_gpio", 0, &flags);
	if (nfc_reset_gpio < 0) { 
			pr_info("Failed to get gpio flags, error: %d\n",nfc_reset_gpio);
	}
	err = gpio_request(nfc_reset_gpio, "nfc_reset_gpio");
	if (err) {
		pr_info("%s: unable to request gpio %d\n", __func__, nfc_reset_gpio);
	}
	err = gpio_direction_output(nfc_reset_gpio, 0);
	if (err) {
		pr_info("gpio_direction_output fail %s\n", "nfc_reset_gpio");
	}
    //////////////////////////////////////////////////////////////////////

	dev_info(&client->dev, "FM17550 i2c function probe entrance");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error!");
		err = -EIO;
		goto exit_err_clean;
	}
    
    dev_err(&client->dev, "read FM17550 device ID \n");
    err = fm_i2c_read(client, 0x37, &data, 1);
    if(!err)
        dev_err(&client->dev, "FM17550 device ID is 0x%x \n", data);
    else
	{
        dev_err(&client->dev, "read FM17550 device ID failed \n");
		goto exit_err_clean;
	}

	if (NULL == fm_client) {
		fm_client = client;
	} else {
		dev_err(&client->dev,
			"this driver does not support multiple clients");
		err = -EBUSY;
		goto exit_err_clean;
	}

	client_data = kzalloc(sizeof(struct fm_client_data),
						GFP_KERNEL);
	if (NULL == client_data) {
		dev_err(&client->dev, "no memory available");
		err = -ENOMEM;
		goto exit_err_clean;
	}
	
	INIT_DELAYED_WORK(&nfc_polling_work, polling_nfc_func);
    pdata = client_data;
	if(driver_create_file(&fm_i2c_driver.driver, &driver_attr_nfc_polling))
	{
		pr_err("ADC1000  1 driver_create_file ERROR \n");
	}
	//FM175XX_HardReset();
	//Pcd_ConfigISOType(0);
	FM175XX_HardPowerdown(1);
	
	nfc_probe = 1;
	
    return err;

exit_err_clean:
	nfc_probe_status = err;
	if (err)
		fm_client = NULL;
	return err;
}

static int fm_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int err = 0;
    dev_info(&client->dev, "========FM17550 suspend==========\n");
    //if(polled_status)
    //    polling_status = 0;
	return err;
}

static int fm_i2c_resume(struct i2c_client *client)
{
	int err = 0;
    //FM175XX_HardReset();
    dev_info(&client->dev, "========FM17550 resume==========\n");
    // if(polled_status)
    // {
        // polling_status = 1;
        // schedule_delayed_work(&nfc_polling_work, HZ/10);
    // }
	return err;
}


static int fm_i2c_remove(struct i2c_client *client)
{
    struct fm_client_data *client_data = i2c_get_clientdata(client);
    kfree(client_data);
    kfree(fm_client);
	fm_client = NULL;
    pdata = NULL;

	wake_lock_destroy(&nfc_opt_lock);

	return 0;
}



static const struct i2c_device_id fm_id[] = {
	{NFC_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, fm_id);

static const struct of_device_id fm17550_of_match[] = {
	{ .compatible = "nfc,fm17550", },
	{ }
};
MODULE_DEVICE_TABLE(of, fm17550_of_match);

static struct i2c_driver fm_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = NFC_NAME,
		.of_match_table = fm17550_of_match,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = fm_id,
	.probe = fm_i2c_probe,
	.remove = fm_i2c_remove,
	.suspend = fm_i2c_suspend,
	.resume = fm_i2c_resume,
};

 static int __init FM_i2c_init(void)
{
	return i2c_add_driver(&fm_i2c_driver);
}

static void __exit FM_i2c_exit(void)
{
	i2c_del_driver(&fm_i2c_driver);
}

MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("driver for " NFC_NAME);
MODULE_LICENSE("GPL v2");

module_init(FM_i2c_init);
module_exit(FM_i2c_exit);

