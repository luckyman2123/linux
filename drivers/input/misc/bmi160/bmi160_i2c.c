/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_i2c.c
 * @date     2014/11/25 14:40
 * @id       "20f77db"
 * @version  1.3
 *
 * @brief
 * This file implements moudle function, which add
 * the driver to I2C core.
*/
//BMI160
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "bmi160_driver.h"
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

//BMI220
/*********************************************************************/
/* system header files */
/*********************************************************************/
//#include <linux/module.h>
//#include <linux/i2c.h>
//#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>

/*********************************************************************/
/* own header files */
/*********************************************************************/
#include "bmi2xy_driver.h"
#include "bs_log.h"

/*! @defgroup bmi160_i2c_src
 *  @brief bmi160 i2c driver module
 @{*/

#define ENABLE_REGULATOR
#define GPIO_VTG_MIN_UV		1800000
#define GPIO_VTG_MAX_UV		1800000

static struct i2c_client *bmi_client;
static int bmiXXX = -1;//0:bmi160  1:bmi220
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
static s8 bmi_i2c_read(struct i2c_client *client, u8 reg_addr,
			u8 *data, u8 len)
	{
#if !defined BMI_USE_BASIC_I2C_FUNC
		s32 dummy;
		if (NULL == client)
			return -EINVAL;

		while (0 != len--) {
#ifdef BMI_SMBUS
			dummy = i2c_smbus_read_byte_data(client, reg_addr);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c smbus read error");
				return -EIO;
			}
			*data = (u8)(dummy & 0xff);
#else
			dummy = i2c_master_send(client, (char *)&reg_addr, 1);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c bus master write error");
				return -EIO;
			}

			dummy = i2c_master_recv(client, (char *)data, 1);
			if (dummy < 0) {
				dev_err(&client->dev, "i2c bus master read error");
				return -EIO;
			}
#endif
			reg_addr++;
			data++;
		}
		return 0;
#else
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

		for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0)
				break;
			else
				usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
		}

		if (BMI_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}

		return 0;
#endif
	}


static s8 bmi_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
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

	for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
	}

	if (BMI_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
}


/* i2c write routine for */
static s8 bmi_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMI_USE_BASIC_I2C_FUNC
	s32 dummy;

#ifndef BMI_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -EPERM;

	while (0 != len--) {
#ifdef BMI_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			dev_err(&client->dev, "error writing i2c bus");
			return -EPERM;
		}

	}
	usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
	BMI_I2C_WRITE_DELAY_TIME * 1000);
	return 0;
#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < BMI_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
				BMI_I2C_WRITE_DELAY_TIME * 1000);
			}
		}
		if (BMI_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	usleep_range(BMI_I2C_WRITE_DELAY_TIME * 1000,
	BMI_I2C_WRITE_DELAY_TIME * 1000);
	return 0;
#endif
}


static s8 bmi_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = bmi_i2c_read(bmi_client, reg_addr, data, len);
	return err;
}

static s8 bmi_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = bmi_i2c_write(bmi_client, reg_addr, data, len);
	return err;
}

s8 bmi_burst_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u16 len)
{
	int err = 0;
	err = bmi_i2c_burst_read(bmi_client, reg_addr, data, len);
	return err;
}
EXPORT_SYMBOL(bmi_burst_read_wrapper);

//BMI220
////////////////////////////////////////////////////////////////////////////////////
/**
 * bmi2xy_i2c_read - The I2C read function.
 *
 * @client : Instance of the I2C client
 * @reg_addr : The register address from where the data is read.
 * @data : The pointer to buffer to return data.
 * @len : The number of bytes to be read
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi2xy_i2c_read(struct i2c_client *client,
	u8 reg_addr, u8 *data, u16 len)
{
	s32 retry;

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
	for (retry = 0; retry < BMI2XY_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		usleep_range(BMI2XY_I2C_WRITE_DELAY_TIME * 1000,
				BMI2XY_I2C_WRITE_DELAY_TIME * 1000);
	}

	if (retry >= BMI2XY_MAX_RETRY_I2C_XFER) {
		PERR("I2C xfer error");
		return -EIO;
	}

	return 0;
}

/**
 * bmi2xy_i2c_read - The I2C write function.
 *
 * @client : Instance of the I2C client
 * @reg_addr : The register address to start writing the data.
 * @data : The pointer to buffer holding data to be written.
 * @len : The number of bytes to write.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi2xy_i2c_write(struct i2c_client *client,
	u8 reg_addr, const u8 *data, u16 len)
{
	s32 retry;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = len + 1,
		.buf = NULL,
	};
	msg.buf = kmalloc(len + 1, GFP_KERNEL);
	if (!msg.buf) {
		PERR("Allocate memory failed\n");
		return -ENOMEM;
	}
	msg.buf[0] = reg_addr;
	memcpy(&msg.buf[1], data, len);
	for (retry = 0; retry < BMI2XY_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, &msg, 1) > 0)
			break;
		usleep_range(BMI2XY_I2C_WRITE_DELAY_TIME * 1000,
				BMI2XY_I2C_WRITE_DELAY_TIME * 1000);
	}
	kfree(msg.buf);
	if (retry >= BMI2XY_MAX_RETRY_I2C_XFER) {
		PERR("I2C xfer error");
		return -EIO;
	}

	return 0;
}

/**
 * bmi2xy_i2c_read_wrapper - The I2C read function pointer used by BMI2xy API.
 *
 * @dev_addr : I2c Device address
 * @reg_addr : The register address to read the data.
 * @data : The pointer to buffer to return data.
 * @len : The number of bytes to be read
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi2xy_i2c_read_wrapper(u8 dev_addr,
	u8 reg_addr, u8 *data, u16 len)
{
	s8 err;

	err = bmi2xy_i2c_read(bmi_client, reg_addr, data, len);
	return err;
}

/**
 * bmi2xy_i2c_write_wrapper - The I2C write function pointer used by BMI2xy API.
 *
 * @dev_addr : I2c Device address
 * @reg_addr : The register address to start writing the data.
 * @data : The pointer to buffer which holds the data to be written.
 * @len : The number of bytes to be written.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static s8 bmi2xy_i2c_write_wrapper(u8 dev_addr,
	u8 reg_addr, const u8 *data, u16 len)
{
	s8 err;

	err = bmi2xy_i2c_write(bmi_client, reg_addr, data, len);
	return err;
}
//////////////////////////////////////////////////////////////////////////////

/*!
 * @brief BMI probe function via i2c bus
 *
 * @param client the pointer of i2c client
 * @param id the pointer of i2c device id
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
//////////////////////////////////////////////////////////
static int codec_power_gpio = 0;
static struct i2c_driver bmi_i2c_driver;
static ssize_t show_driver_attr_codec_power(struct device_driver *ddri, char *buf)
{
	int value = -1;
    value = gpio_get_value(codec_power_gpio);
	return snprintf(buf, 20, "%d\n", value);
}
static ssize_t store_driver_attr_codec_power(struct device_driver *ddri, const char *buf, size_t count)
{
	if(buf[0] == '0')
    {
        pr_err("codec power off\n");
        //gpio_direction_output(codec_power_gpio, 0);
    }
	else
    {
        gpio_direction_output(codec_power_gpio, 1);
        pr_err("codec power on\n");
    }
	return count;
}
static DRIVER_ATTR(codec_power,	S_IWUSR | S_IRUGO, show_driver_attr_codec_power, store_driver_attr_codec_power);

/////////////////////////////////////////////////////////
//BMI160
static int bmi_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
		int err = 0;
		struct bmi_client_data *client_data = NULL;

		dev_info(&client->dev, "BMI160 i2c function probe entrance");
		bmiXXX = 0;
		
		client_data = kzalloc(sizeof(struct bmi_client_data),
							GFP_KERNEL);
		if (NULL == client_data) {
			dev_err(&client->dev, "no memory available");
			err = -ENOMEM;
			goto exit_err_clean1;
		}
		
		client_data->device.bus_read = bmi_i2c_read_wrapper;
		client_data->device.bus_write = bmi_i2c_write_wrapper;
#ifdef ENABLE_REGULATOR		
		client_data->vdd = regulator_get(&client->dev, "vdd");

		if (regulator_count_voltages(client_data->vdd) > 0) {
			err = regulator_set_voltage(client_data->vdd, GPIO_VTG_MIN_UV, GPIO_VTG_MAX_UV);
			if (err)
				dev_info(&client->dev,"Regulator set_vtg failed retval rc = %d\n", err);
		}

		err = regulator_enable(client_data->vdd);
		if (err) {
			dev_err(&client->dev, "Regulator client_data->vdd enable failed err=%d\n", err);
			err = regulator_disable(client_data->vdd);
			return err;
		}
		else {
			dev_info(&client->dev, "Regulator client_data->vdd enable success!\n");
		}
#endif
		if(driver_create_file(&bmi_i2c_driver.driver, &driver_attr_codec_power))
		{
			pr_err("codec_power  driver_create_file ERROR \n");
		}
        pr_err("bmi_probe enter\n");
		return bmi_probe(client_data, &client->dev);

exit_err_clean1:
		if (err)
			bmi_client = NULL;
		return err;
}
///////////////////////////////////////////////////
//BMI220
/**
 * bmi2xy_i2c_probe - The I2C probe function called by I2C bus driver.
 *
 * @client : The I2C client instance
 * @id : The I2C device ID instance
 *
 * Return : Status of the function.
 * * 0 - OK
 * * negative value - Error.
 */
static int bmi2xy_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;
	struct bmi2xy_client_data *client_data = NULL;
	
	PDEBUG("entrance");
	bmiXXX = 1;
	client_data = kzalloc(sizeof(struct bmi2xy_client_data),
						GFP_KERNEL);
	if (client_data == NULL) {
		PERR("no memory available");
		err = -ENOMEM;
		goto exit_err_clean2;
	}
	/* h/w init */
	client_data->device.read_write_len = 256;
	client_data->device.intf = BMI2_I2C_INTERFACE;
	client_data->device.read = bmi2xy_i2c_read_wrapper;
	client_data->device.write = bmi2xy_i2c_write_wrapper;
#ifdef ENABLE_REGULATOR		
		client_data->vdd = regulator_get(&client->dev, "vdd");

		if (regulator_count_voltages(client_data->vdd) > 0) {
			err = regulator_set_voltage(client_data->vdd, GPIO_VTG_MIN_UV, GPIO_VTG_MAX_UV);
			if (err)
				dev_info(&client->dev,"Regulator set_vtg failed retval rc = %d\n", err);
		}

		err = regulator_enable(client_data->vdd);
		if (err) {
			dev_err(&client->dev, "Regulator client_data->vdd enable failed err=%d\n", err);
			err = regulator_disable(client_data->vdd);
			return err;
		}
		else {
			dev_info(&client->dev, "Regulator client_data->vdd enable success!\n");
		}
#endif
	if(driver_create_file(&bmi_i2c_driver.driver, &driver_attr_codec_power))
	{
		pr_err("codec_power  driver_create_file ERROR \n");
	}
	
    pr_err("bmi_probe enter\n");
	return bmi2xy_probe(client_data, &client->dev);

exit_err_clean2:
	if (err)
		bmi_client = NULL;
	return err;
}

/////////////////////////////////////////////////////////
static int bmi_i2c_common_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
		int err = 0;
		u8 chip_id = 0xff;
	//////////////////////////////////////////////////////////////////////
		enum of_gpio_flags flags;
		codec_power_gpio = of_get_gpio_flags(client->dev.of_node, 0, &flags);
		if (codec_power_gpio < 0) { 
				pr_info("Failed to get gpio flags, error: %d\n",codec_power_gpio);
		}
		err = gpio_request(codec_power_gpio, "codec_power_gpio");
		if (err) {
			pr_info("%s: unable to request gpio %d\n", __func__, codec_power_gpio);
		}

		err = gpio_direction_output(codec_power_gpio, 1);
		if (err) {
			pr_info("gpio_direction_output fail %s\n", "codec_power_gpio");
		}
        pr_err("codec power on\n");
        msleep(200);
	//////////////////////////////////////////////////////////////////////
		if (NULL == bmi_client) {
			bmi_client = client;
		} else {
			dev_err(&client->dev,
				"this driver does not support multiple clients");
			err = -EBUSY;
			goto exit_err_clean0;
		}
		
		if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
			dev_err(&client->dev, "i2c_check_functionality error!");
			err = -EIO;
			goto exit_err_clean0;
		}
		
		err = bmi_i2c_read(client,0x00,&chip_id,1);
		pr_err("chip_id = 0x%02x,err = %d",chip_id,err);
		if(SENSOR_CHIP_ID_BMI == chip_id || SENSOR_CHIP_ID_BMI_C2 == chip_id || SENSOR_CHIP_ID_BMI_C3 == chip_id)
			return bmi_i2c_probe(client,id);
		else if(BMI220_CHIP_ID == chip_id)
			return bmi2xy_i2c_probe(client,id);
		else
			return -1;
		
exit_err_clean0:
		if (err)
			bmi_client = NULL;
		return err;
}

static int bmi_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int err = 0;
	pr_err("bmi_i2c_suspend\n");
	if(0 == bmiXXX)
		err = bmi_suspend(&client->dev);
	else if(1 == bmiXXX)
		err = bmi2xy_suspend(&client->dev);
	else
		return -1;
	
	gpio_direction_output(codec_power_gpio,0);
	return err;
}

static int bmi_i2c_resume(struct i2c_client *client)
{
	int err = 0;
	gpio_direction_output(codec_power_gpio,1);
	msleep(100);
	pr_err("bmi_i2c_resume\n");
	if(0 == bmiXXX)
		err = bmi_resume(&client->dev);
	else if(1 == bmiXXX)
		err = bmi2xy_resume(&client->dev);
	else
		return -1;

	return err;
}


static int bmi_i2c_remove(struct i2c_client *client)
{
	int err = 0;
	if(0 == bmiXXX)
		err = bmi_remove(&client->dev);
	else if(1 == bmiXXX)
		err = bmi2xy_remove(&client->dev);
	else
		return -1;
	
	bmi_client = NULL;

	return err;
}



static const struct i2c_device_id bmi_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmi_id);

static const struct of_device_id bmi160_of_match[] = {
	{ .compatible = "bosch-sensortec,bmi160", },
	{ .compatible = "bmi160", },
	{ .compatible = "bosch, bmi160", },
	{ }
};
MODULE_DEVICE_TABLE(of, bmi160_of_match);

static struct i2c_driver bmi_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		.of_match_table = bmi160_of_match,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = bmi_id,
	//.probe = bmi_i2c_probe,
	.probe = bmi_i2c_common_probe,
	.remove = bmi_i2c_remove,
	.suspend = bmi_i2c_suspend,
	.resume = bmi_i2c_resume,
};

 static int __init BMI_i2c_init(void)
{
	return i2c_add_driver(&bmi_i2c_driver);
}

static void __exit BMI_i2c_exit(void)
{
	i2c_del_driver(&bmi_i2c_driver);
}

MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("driver for " SENSOR_NAME);
MODULE_LICENSE("GPL v2");

module_init(BMI_i2c_init);
module_exit(BMI_i2c_exit);

