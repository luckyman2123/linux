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
#include "fm17750_i2c.h"
#include "FM175XX.h"
#include "FM175XX_REG.h"
#include "MIFARE.h"
#include "READER_API.h"
#include <linux/regulator/consumer.h>


/*! @defgroup fm17550_i2c_src
 *  @brief fm17550 i2c driver module
 @{*/
#define FM_MAX_RETRY_I2C_XFER (10)
#define FM_I2C_WRITE_DELAY_TIME (1)

struct i2c_client *fm_client;
int nfc_reset_gpio = 0;
struct fm_client_data *pdata;
struct delayed_work nfc_polling_work;
unsigned char cardA_data[6] = {0};
unsigned char cardB_data[14] = {0};
unsigned char timesA = 0;
unsigned char timesB = 0;

//static struct mutex lock_this;
static struct mutex lock_polling;


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
/*!
 * @brief define i2c wirte function
 *
 * @param client the pointer of i2c client
 * @param reg_addr register address
 * @param data the pointer of data buffer
 * @param len block size need to write
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/

/*	i2c read routine for API*/
s8 fm_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
	int retry;
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
    
    //mutex_lock(&lock_this);
	for (retry = 0; retry < FM_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg,
					ARRAY_SIZE(msg)) > 0)
			break;
		else
			usleep_range(FM_I2C_WRITE_DELAY_TIME * 1000,
			FM_I2C_WRITE_DELAY_TIME * 1000);
	}
    //mutex_unlock(&lock_this);

	if (FM_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
}
/* i2c write routine for */
s8 fm_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
	u8 *buffer;
	int retry;
    buffer = kmalloc(len+1,GFP_KERNEL);
    buffer[0] = reg_addr;
    memcpy(buffer+1,data,len);
    //mutex_lock(&lock_this);
    struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len =len + 1,
		 .buf = buffer,
		},
	};
	
    //buffer = kmalloc(len+1,GFP_KERNEL);
    //buffer[0] = reg_addr;
    //memcpy(buffer+1,data,len);
    
	for (retry = 0; retry < FM_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg,
					ARRAY_SIZE(msg)) > 0) {
			break;
		} else {
			usleep_range(FM_I2C_WRITE_DELAY_TIME * 1000,
			FM_I2C_WRITE_DELAY_TIME * 1000);
		}
	}
    kfree(buffer);
    //mutex_unlock(&lock_this);
	if (FM_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}
	return 0;
}

void clear_card_status(void)
{
	cardA_data[1] = 0;
	cardB_data[1] = 0;
}

/*!
 * @brief FM probe function via i2c bus
 *
 * @param client the pointer of i2c client
 * @param id the pointer of i2c device id
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/

/////////////////TYPE A&B EVENT///////////////////
unsigned char TYPE_A_EVENT(void)
{
    unsigned char result;
    SetRf(0);
    FM175XX_Initial_ReaderA();
    SetRf(3);	
    SetReg(0x27,0xF8);
    SetReg(0x28,0x3F);
    result = ReaderA_CardActivate();
    if(result != FM175XX_SUCCESS)
    {
        SetRf(0);
        if(polling_mode)
        {
            if(cardA_data[1] != 2)
            {
                cardA_data[1] = 2;
                memset((cardA_data+2),0,4);
                send_nfc_msg2(cardA_data,sizeof(cardA_data));
            }
        }
        dev_info(&fm_client->dev, "---------------------------ReaderA_CardActivate failed!\n");
        return CMD_CARDAERR;
    }
    //dev_info(&fm_client->dev, "find cardA: %02X %02X %02X %02X\n",Type_A.UID[0],Type_A.UID[1],Type_A.UID[2],Type_A.UID[3]);
    if(polling_mode)
    {
        if(cardA_data[1] != 1)
        {
            cardA_data[1] = 1;
            memcpy((cardA_data+2),Type_A.UID,4);
            send_nfc_msg2(cardA_data,sizeof(cardA_data));
        }
        else if(memcmp((cardA_data+2),Type_A.UID,4))
        {
            memcpy((cardA_data+2),Type_A.UID,4);
            send_nfc_msg2(cardA_data,sizeof(cardA_data));
        }
    }
    if(have_cmd)
	{
		have_cmd = 0;
        handle_nfc_cmd(cmd);
        //result = (*(cmd_handle[cmd]))();
	}
    SetRf(0);	
    return 0;
}
unsigned char TYPE_B_EVENT(void)
{
    unsigned char result;
    //dev_info(&fm_client->dev, "-> TYPE B CARD!\n");
    SetRf(0);			
    FM175XX_Initial_ReaderB();
    SetRf(3);		
    	
    result = ReaderB_Request();
    if(result != FM175XX_SUCCESS)
    {
        SetRf(0);
        if(cardB_data[1] != 2)
        {
            cardB_data[1] = 2;
            memset((cardB_data+2),0,12);
            send_nfc_msg2(cardB_data,sizeof(cardB_data));
        }
        return CMD_CARDAERR;
    }
    
    if(polling_mode)
    {
        if(cardB_data[1] != 1)
        {
            cardB_data[1] = 1;
            memset((cardB_data+2),0,12);
            memcpy((cardB_data+2),Type_B.ATQB,12);
            send_nfc_msg2(cardB_data,sizeof(cardB_data));
        }
        else if(memcmp((cardB_data+2),Type_B.ATQB,12))
        {
            memcpy((cardB_data+2),Type_B.ATQB,12);
            send_nfc_msg2(cardB_data,sizeof(cardB_data));
        }
    }
    ///*
    result = ReaderB_Attrib();
    if(result != FM175XX_SUCCESS)
    {
		dev_info(&fm_client->dev, "ReaderB_Attrib failed\n");
        SetRf(0);
        return;
    }//*/
    if(have_cmd)
	{
		have_cmd = 0;
        handle_nfc_cmd(cmd);
        //result = (*(cmd_handle[cmd]))();
	}
    SetRf(0);	
    return 0;
}

static bool polling_status = 0;
static bool polled_status = 0;
static void polling_nfc_func(struct work_struct *work)
{
    unsigned char polling_card;
    //mutex_lock(&lock_polling);
    SetRf(3);			
    if(FM175XX_Polling(&polling_card)== SUCCESS)
    {
        if(polling_card & 0x01)//TYPE A
        {
            timesA = 0;
            TYPE_A_EVENT();							
        }
        else if(cardA_data[1] && polling_mode)
        {
            if(timesA >= 2)
            {
                cardA_data[1] = 0;
                send_nfc_msg2(cardA_data,sizeof(cardA_data));
            }
            else
            {
                timesA ++;
            }
        }
        
        if(polling_card & 0x02)
        {
            timesB = 0;
            TYPE_B_EVENT();
        }
        else if(cardB_data[1] && polling_mode)
        {
            if(timesB >= 2)
            {
                cardB_data[1] = 0;
                send_nfc_msg2(cardB_data,sizeof(cardB_data));
            }
            else
                timesB ++;
        }
    }
    else
    {
        if(cardA_data[1] && polling_mode)
        {
            if(timesA >= 2)
            {
                cardA_data[1] = 0;
                send_nfc_msg2(cardA_data,sizeof(cardA_data));
            }
            else
            {
                timesA ++;
            }
        }
        if(cardB_data[1] && polling_mode)
        {
            if(timesB >= 2)
            {
                cardB_data[1] = 0;
                send_nfc_msg2(cardB_data,sizeof(cardB_data));
            }
            else
                timesB ++;
        }
    }
    SetRf(0);
    //mutex_unlock(&lock_polling);
    if(polling_mode)
        schedule_delayed_work(&nfc_polling_work, HZ/10);
}

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
    pdata = client_data;
    
    ////////////////////////////nrf init/////////////////////////////////////////
    INIT_DELAYED_WORK(&nfc_polling_work, polling_nfc_func);
    //mutex_init(&lock_this);
    mutex_init(&lock_polling);
    cardA_data[0] = NFC_KARDA_REPORT;
    cardB_data[0] = NFC_KARDB_REPORT;
    FM175XX_HardReset();
    /////////////////////////////////////////////////////////////////////////////
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
    if(polled_status)
        polling_status = 0;
	return err;
}

static int fm_i2c_resume(struct i2c_client *client)
{
	int err = 0;
    FM175XX_HardReset();
    dev_info(&client->dev, "========FM17550 resume==========\n");
    if(polled_status)
    {
        polling_status = 1;
        schedule_delayed_work(&nfc_polling_work, HZ/10);
    }
	return err;
}


static int fm_i2c_remove(struct i2c_client *client)
{
    struct fm_client_data *client_data = i2c_get_clientdata(client);
    kfree(client_data);
    kfree(fm_client);
	fm_client = NULL;
    pdata = NULL;

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

