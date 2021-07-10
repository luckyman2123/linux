/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename fm17550_driver.h
 * @date     2015/08/17 14:40
 * @id       "2d9bd33"
 * @version  1.3
 *
 * @brief
 * The head file of FM17550 device driver core code
*/
#ifndef _FM17550_DRIVER_H
#define _FM17550_DRIVER_H

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "fm17750.h"
#include "nfc_cmd_handle.h"

/* sensor specific */
#define NFC_NAME "fm17550"

#ifndef SUCCESS
#define SUCCESS (0)
#endif

#ifndef ERROR
#define ERROR (1)
#endif

///////////////////////////////////netlink//////////////////////////
int send_nfc_msg(unsigned char *message,int msg_size);
int send_nfc_msg2(unsigned char *message,int msg_size);
void app_enable_nfc(bool enable);
void clear_card_status(void);
////////////////////////////////////////////////

/*! fm17550 sensor error status */
struct err_status {
	u8 fatal_err;
	u8 err_code;
	u8 i2c_fail;
	u8 drop_cmd;
	u8 mag_drdy_err;
	u8 err_st_all;
};

struct fm_client_data {
	struct device *dev;
	//struct input_dev *input;/*acc_device*/
	struct delayed_work work;
	struct delayed_work gyro_work;
	struct work_struct irq_work;
	struct wake_lock wake_lock;

	u8 chip_id;
	struct err_status err_st;
	unsigned char fifo_int_tag_en;

	unsigned char *fifo_data;
	uint16_t gpio_pin;
	struct mutex mutex_op_mode;
	struct mutex mutex_enable;
	int IRQ;
};

s8 fm_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len);
s8 fm_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len);
void send_msg(char *s, unsigned char *value, unsigned char len);
unsigned char TYPE_A_EVENT(void);
unsigned char TYPE_B_EVENT(void);
#endif/*_FM17550_DRIVER_H*/
/*@}*/

