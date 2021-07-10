/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename gps.c
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
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#endif
#include <linux/regulator/consumer.h>


#include <linux/interrupt.h> 
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>



#include "mg_netlink.h"

#define GPS_NAME "beidou,gps"
#define GPS_I2C_WRITE_DELAY_TIME (1)

/*	i2c read routine for API*/
static struct i2c_client *gps_i2c_client = NULL;
static struct mutex lock_this;

static bool polling_mode = 0;

static int inter = 0;






IIC_GPS_RESP gspresp; 



struct delayed_work gps_polling_work;

static unsigned char *pd = NULL;

static bool gps_probe = 0;

s8 gps_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u16 len)
{
	int retry;
	int ret;


	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};
    
    mutex_lock(&lock_this);
	for (retry = 0; retry < 5; retry++) {
		ret = i2c_transfer(client->adapter, msg,ARRAY_SIZE(msg));
		if(ret >0)
			break;
		else
			usleep_range(GPS_I2C_WRITE_DELAY_TIME * 1000,GPS_I2C_WRITE_DELAY_TIME * 1000);
	}
    mutex_unlock(&lock_this);

	if (5 <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		printk("gps_i2c_read   EIO-------------------------------------------------------------------------\n");
		return -EIO;
	}

	return 0;
}
/* i2c write routine for */
s8 gps_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
	u8 *buffer;
	int retry;
    
    mutex_lock(&lock_this);
    struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len =len + 1,
		 .buf = buffer,
		},
	};
	
    buffer = kmalloc(len+1,GFP_KERNEL);
    buffer[0] = reg_addr;
    memcpy(buffer+1,data,len);
    
	for (retry = 0; retry < 5; retry++) {
		if (i2c_transfer(client->adapter, msg,
					ARRAY_SIZE(msg)) > 0) {
			break;
		} else {
			usleep_range(GPS_I2C_WRITE_DELAY_TIME * 1000,
			GPS_I2C_WRITE_DELAY_TIME * 1000);
		}
	}
    kfree(buffer);
    mutex_unlock(&lock_this);
	if (5 <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}
	return 0;
}

/*!
 * @brief GPS probe function via i2c bus
 *
 * @param client the pointer of i2c client
 * @param id the pointer of i2c device id
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/

void netlink_gps_cmd_handle(unsigned char *msg)
{

	PIIC_GSP_REQ req;

	req = (PIIC_GSP_REQ)msg;


	if(!gps_probe)
	{
		gspresp.status =  NETLINK_NODRIV;
	}
	else if(GPS_POLLING_ON == req->cmd)
	{
		if(!polling_mode)
		schedule_delayed_work(&gps_polling_work, HZ);
		
		polling_mode = 1;
		gspresp.status = NETLINK_SUCCESS;
	}
	else if(GPS_POLLING_OFF == req->cmd)
	{
		polling_mode = 0;
		gspresp.status = NETLINK_SUCCESS;
	}

	gspresp.len = 3;
	send_mg_msg((unsigned char *)&gspresp,gspresp.len);
}





static void polling_gps_func(struct work_struct *work)
{


	int i = 0,j = 0,d = 0;
	 memset(pd, 0, sizeof(pd));

	char *ptrgga,*ptrrmc,*ptrvtg,*ptrttxt;
	

	gps_i2c_read(gps_i2c_client,0x00,pd,1500);


	gspresp.status = NETLINK_SUCCESS;
	gspresp.len = 1500;
	strncpy(gspresp.data,pd,1500);
	send_mg_msg2((unsigned char *)&gspresp,gspresp.len);
	
	
	 if(polling_mode)
        schedule_delayed_work(&gps_polling_work, HZ);
  
}







/*******************polling node**********************************/
static struct i2c_driver gps_i2c_driver;
//read
static ssize_t show_driver_attr_gps_polling(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, 20, "%d\n", polling_mode);
}

static ssize_t store_driver_attr_gps_polling(struct device_driver *ddri, const char *buf, size_t count)
{
	if(buf[0] == '1'){
		// polling_mode = 1;
		//schedule_delayed_work(&gps_polling_work, HZ);
		enable_irq(inter);
		
	}else{
		// polling_mode = 0;
		disable_irq(inter);
	}
	return count;
}
static DRIVER_ATTR(gps_polling,	S_IWUSR | S_IRUGO, show_driver_attr_gps_polling, store_driver_attr_gps_polling);
/****************************************************************/




// 下半部函数
void func(struct work_struct *work)
{		
	int i = 0,j = 0,d = 0;
	 memset(pd, 0, sizeof(pd));

	char *ptrgga,*ptrrmc,*ptrvtg,*ptrttxt;
	
	gps_i2c_read(gps_i2c_client,0x00,pd,1500);

	gspresp.status = NETLINK_SUCCESS;
	gspresp.len = 1500;
	strncpy(gspresp.data,pd,1500);
	send_mg_msg2((unsigned char *)&gspresp,gspresp.len);

}





DECLARE_WORK(mywork, func);




irqreturn_t irq_handler(int irqno, void *dev_id)
 
{

	schedule_work(&mywork);
	
	return IRQ_HANDLED;
}




//////probe/////////
static int gps_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	unsigned char value = 0;
	int ret = 0;
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error!");
		return -EIO;
	}



	
	if(client)
		gps_i2c_client = client;
	else
		return -EIO;
	
	mutex_init(&lock_this);
	
	if(gps_i2c_read(client,0x00,&value,1))
		return -EIO;

	
	pd = kmalloc(1500,GFP_KERNEL);
	if(!pd)
	{
		pr_err("gps kmalloc failed\n");
		return -EIO;
	}


	gspresp.type = CMD_GPS;
	
	//pd[0] = CMD_GPS;
	//pd[1] = 0x05;
	//pd[2] = 0xE0;//0x05E0 = 1504
	//pd[3] = GPS_REPORT;
	
    ////////////////////////////nrf init/////////////////////////////////////////
    // INIT_DELAYED_WORK(&gps_polling_work, polling_gps_func);


    /////////////////////////////////////////////////////////////////////////////
	if(driver_create_file(&gps_i2c_driver.driver, &driver_attr_gps_polling))
	{
		dev_err(&client->dev, "creat gps_polling node failed \n");
	}
	
	gps_probe = 1;




	//以下是中断的方式
	ret = gpio_request(2, "fpga_key_inter");
	if(ret){
		printk("1111111111111111111111111111gpio_request() failed !\n");

	    return -1;
	}


	

	ret = gpio_direction_input(2);
	if(ret){
		printk("2gpio_direction_input() failed !\n");
		
	}
	inter = gpio_to_irq(2);
	if(inter < 0){
		printk("gpio_to_irq() failed !\n");
		ret = inter;
			
	}
	printk("inter = %d\n", inter);
	ret = request_irq(inter, irq_handler,  IRQF_TRIGGER_HIGH, "fpga_key_inter",NULL); 
	if(ret){
		printk("request_irq() failed ! %d\n", ret);
		
	}

	disable_irq(inter);
    printk("fpga_key_to_app_init(void)--\n");      



	
    return 0;
}

static int gps_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	// int err = 0;
    // dev_info(&client->dev, "========GPS17550 suspend==========\n");
    // if(polled_status)
        // polling_status = 0;
	// return err;
	return 0;
}

static int gps_i2c_resume(struct i2c_client *client)
{
	// int err = 0;
    // GPS175XX_HardReset();
    // dev_info(&client->dev, "========GPS17550 resume==========\n");
    // if(polled_status)
    // {
        // polling_status = 1;
        // schedule_delayed_work(&gps_polling_work, HZ*3);
    // }
	// return err;
	return 0;
}


static int gps_i2c_remove(struct i2c_client *client)
{
    kfree(client);
	kfree(pd);
	pd = NULL;
    client = NULL;

	return 0;
}



static const struct i2c_device_id gps_id[] = {
	{GPS_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, gps_id);

static const struct of_device_id beidou_gps_of_match[] = {
	{ .compatible = "beidou,gps", },
	{ }
};
MODULE_DEVICE_TABLE(of, beidou_gps_of_match);

static struct i2c_driver gps_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = GPS_NAME,
		.of_match_table = beidou_gps_of_match,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = gps_id,
	.probe = gps_i2c_probe,
	.remove = gps_i2c_remove,
	.suspend = gps_i2c_suspend,
	.resume = gps_i2c_resume,
};

 static int __init GPS_i2c_init(void)
{

	return i2c_add_driver(&gps_i2c_driver);
}

static void __exit GPS_i2c_exit(void)
{
	kfree(pd);
	pd = NULL;
	i2c_del_driver(&gps_i2c_driver);
}

MODULE_AUTHOR("zxw");
MODULE_DESCRIPTION("driver for " GPS_NAME);
MODULE_LICENSE("GPL v2");

module_init(GPS_i2c_init);
module_exit(GPS_i2c_exit);


