/**
 * @section LICENSE
 * (C) Copyright 2011~2018 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi2xy_driver.h
 * @date     28/01/2020
 * @version  1.34.0
 *
 * @brief    BMI2xy Linux Driver
 */

#ifndef BMI2XY_DRIVER_H
#define BMI2XY_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************/
/* System header files */
/*********************************************************************/
#include <linux/types.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/firmware.h>

/*********************************************************************/
/* Own header files */
/*********************************************************************/
/* BMI2xy variants. Only one should be enabled */
#include "bmi220.h"
#include "bmi_common.h"
#pragma message("building for bmi220")

/*********************************************************************/
/* Macro definitions */
/*********************************************************************/
/** Name of the device driver and accel input device*/
//#define SENSOR_NAME "bmi2xy_acc"
/** Name of the feature input device*/
#define SENSOR_NAME_FEAT "bmi2xy_feat"

/* Generic */
#define ANY_NO_MOTION_WORKAROUND		(1)
#define BMI2XY_ENABLE_INT1			(1)
#define BMI2XY_MAX_RETRY_I2C_XFER		(10)
#define BMI2XY_I2C_WRITE_DELAY_TIME		(1)
#define REL_FEAT_STATUS				(1)
#define REL_HW_STATUS				(2)

/*fifo definition*/
//#define A_BYTES_FRM      (6)
//#define G_BYTES_FRM      (6)
//#define M_BYTES_FRM      (8)
//#define MA_BYTES_FRM     (14)
//#define MG_BYTES_FRM     (14)
#define AG_BYTES_FRM     (12)
#define AMG_BYTES_FRM    (20)
#define FIFO_ACC_EN_MSK				(0x40)
#define FIFO_GYRO_EN_MSK			(0x80)
#define FIFO_MAG_EN_MSK				(0x20)
#define BMI2_ANY_MOT_INT_MASK			(0x40)
#define BMI2_NO_MOT_INT_MASK			(0x20)

/**
 * enum bmi2xy_config_func - Enumerations to select the sensors
 */
enum bmi2xy_config_func {
	BMI2XY_SIG_MOTION_SENSOR = 0,
	BMI2XY_STEP_DETECTOR_SENSOR = 1,
	BMI2XY_STEP_COUNTER_SENSOR = 2,
	BMI2XY_TILT_SENSOR = 3,
	BMI2XY_PICKUP_SENSOR = 4,
	BMI2XY_GLANCE_DETECTOR_SENSOR = 5,
	BMI2XY_WAKEUP_SENSOR = 6,
	BMI2XY_ANY_MOTION_SENSOR = 7,
	BMI2XY_ORIENTATION_SENSOR = 8,
	BMI2XY_FLAT_SENSOR = 9,
	BMI2XY_HIGH_G_SENSOR = 10,
	BMI2XY_LOW_G_SENSOR = 11,
	BMI2XY_ACTIVITY_SENSOR = 12,
	BMI2XY_NO_MOTION_SENSOR = 13,
	BMI2XY_S4S = 14
};

/**
 * enum bmi2xy_int_status0 - Enumerations corresponding to status0 registers
 */
enum bmi2xy_int_status0 {
	SIG_MOTION_OUT = 0x01,
	STEP_DET_OUT = 0x02,
	TILT_OUT = 0x04,
	PICKUP_OUT = 0x08,
	GLANCE_OUT = 0x10,
	WAKEUP_OUT = 0x20,
	ANY_NO_MOTION_OUT = 0x40,
	ORIENTATION_OUT = 0x80
};

/**
 * enum bmi2xy_int_status1 - Enumerations corresponding to status1 registers
 */
enum bmi2xy_int_status1 {
	FIFOFULL_OUT = 0x01,
	FIFOWATERMARK_OUT = 0x02,
	MAG_DRDY_OUT = 0x20,
	ACC_DRDY_OUT = 0x80
};

/**
 * enum bmi2xy_self_test_rslt - Enumerations for Self-test feature
 */
enum bmi2xy_self_test_rslt {
	SELF_TEST_PASS,
	SELF_TEST_FAIL,
	SELF_TEST_NOT_RUN
};

/**
 * struct pw_mode - Structure for sensor power modes.
 */
 /*
struct pw_mode {
	u8 acc_pm;
	u8 mag_pm;
	u8 gyro_pm;
};
*/
/**
 *  struct bmi2xy_client_data - Client structure which holds sensor-specific
 *  information.
 */
struct bmi2xy_client_data {
	struct bmi2_dev device;
	struct device *dev;
	struct input_dev *acc_input;
	struct input_dev *feat_input;
	u8 config_file_loaded;
	struct regulator *vdd;
#ifdef FIFO_LOGGING
	struct workqueue_struct *fifo_workqueue;
	char *fifo_data_buf;
	u16 fifo_data_len;
	struct work_struct work;
	struct hrtimer timer;
	ktime_t ktime;
	u8 timer_running;
#endif
	u8 fifo_mag_enable;
	u8 fifo_gyro_enable;
	u8 fifo_acc_enable;
	u32 fifo_bytecount;
	struct pw_mode pw;
	u8 acc_odr;
	u8 gyro_odr;
	u8 mag_odr;
	u8 mag_chip_id;
	struct mutex lock;
	unsigned int IRQ;
	u8 gpio_pin;
	struct work_struct irq_work;
	u16 fw_version;
	char *config_stream_name;
	unsigned int reg_sel;
	unsigned int reg_len;
	unsigned int feat_page_sel;
	unsigned int feat_page_len;
	struct wake_lock wakelock;
	struct delayed_work delay_work_sig;
	atomic_t in_suspend;
	u8 tap_type;
	u8 accel_selftest;
	u8 gyro_selftest;
	u8 sigmotion_enable;
	u8 stepdet_enable;
	u8 stepcounter_enable;
	u8 tilt_enable;
	u8 uphold_to_wake;
	u8 glance_enable;
	u8 wakeup_enable;
	u8 anymotion_enable;
	u8 nomotion_enable;
	u8 orientation_enable;
	u8 flat_enable;
	u8 tap_enable;
	u8 highg_enable;
	u8 s4s_enable;
	u8 high_g_out;
	u8 lowg_enable;
	u8 activity_enable;
	u8 err_int_trigger_num;
	u8 orientation_output;
	u8 activity_output;
	u32 step_counter_val;
	u32 step_counter_temp;
};

/*********************************************************************/
/* Function prototype declarations */
/*********************************************************************/

/**
 * bmi2xy_probe - This is the probe function for bmi2xy sensor.
 * Called from the I2C driver probe function to initialize the sensor.
 *
 * @client_data : Structure instance of client data.
 * @dev : Structure instance of device.
 *
 * Return : Result of execution status
 * * 0 - Success
 * * negative value -> Error
 */
int bmi2xy_probe(struct bmi2xy_client_data *client_data, struct device *dev);

/**
 * bmi2xy_suspend - This function puts the driver and device to suspend mode.
 *
 * @dev : Structure instance of device.
 *
 * Return : Result of execution status
 * * 0 - Success
 * * negative value -> Error
 */
int bmi2xy_suspend(struct device *dev);

/**
 * bmi2xy_resume - This function is used to bring back device from suspend mode.
 *
 * @dev  : Structure instance of device.
 *
 * Return : Result of execution status
 * * 0 - Success
 * * negative value -> Error
 */
int bmi2xy_resume(struct device *dev);

/**
 * bmi2xy_remove - This function removes the driver from the device.
 *
 * @dev : Structure instance of device.
 *
 * Return : Result of execution status
 * * 0 - Success
 * * negative value -> Error
 */
int bmi2xy_remove(struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMI2XY_DRIVER_H_  */
