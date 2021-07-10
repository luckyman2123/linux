/*
 * @section LICENSE
 * (C) Copyright 2011~2018 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi2xy_driver.c
 * @date     28/01/2020
 * @version  1.34.0
 *
 * @brief    BMI2xy Linux Driver
 */

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
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>



/*********************************************************************/
/* Own header files */
/*********************************************************************/
#include "bmi2xy_driver.h"
#include "bs_log.h"

/*********************************************************************/
/* Local macro definitions */
/*********************************************************************/
#define DRIVER_VERSION "1.34.0"

/*********************************************************************/
/* Global data */
/*********************************************************************/

#define MS_TO_US(msec)		UINT32_C((msec) * 1000)

/**
 * soft_reset_store - sysfs write callback which performs the
 * soft rest in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t soft_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long soft_reset;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	/* Base of decimal number system is 10 */
	err = kstrtoul(buf, 10, &soft_reset);

	if (err) {
		PERR("Soft reset: invalid input");
		return err;
	}

	if (soft_reset)
		/* Perform soft reset */
		err = bmi2_soft_reset(&client_data->device);

	if (err) {
		PERR("Soft Reset failed");
		return -EIO;
	}

	return count;
}

/**
 * bmi2xy_i2c_delay_us - Adds a delay in units of microsecs.
 *
 * @usec: Delay value in microsecs.
 */
static void bmi2xy_i2c_delay_us(u32 usec)
{

	if (usec <= (MS_TO_US(20)))

		/* Delay range of usec to usec + 1 millisecs
		 * required due to kernel limitation
		 */
		usleep_range(usec, usec + 1000);
	else
		//usleep(usec);
		msleep(usec/1000);
}

/**
 * chip_id_show - sysfs callback for reading the chip id of the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 chip_id[2] = {0};
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = bmi2_get_regs(BMI2_CHIP_ID_ADDR, chip_id,
					2, &client_data->device);
	if (err) {
		PERR("failed");
		return err;
	}
	return snprintf(buf, 96, "chip_id=0x%x rev_id=0x%x\n",
		chip_id[0], chip_id[1]);
}

/**
 * enable_show - sysfs callback which tells whether accelerometer is
 * enabled or disabled.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	u8 acc_enable = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = bmi2_get_regs(BMI2_PWR_CTRL_ADDR, &acc_enable, 1,
							&client_data->device);
	/* Get the accel enable bit */
	acc_enable = (acc_enable & 0x04) >> 2;
	if (err) {
		PERR("read failed");
		return err;
	}

	return snprintf(buf, 96, "%d\n", acc_enable);
}

/**
 * enable_store - sysfs callback which enables or disables the
 * accelerometer.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 * Accelerometer will not be disabled unless all the features related to
 * accelerometer are disabled.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long op_mode;
	u8 sens_list = BMI2_ACCEL;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;
	if (op_mode == 0 &&
			(client_data->sigmotion_enable == 0) &&
			(client_data->stepdet_enable == 0) &&
			(client_data->stepcounter_enable == 0) &&
			(client_data->wakeup_enable == 0) &&
			(client_data->activity_enable == 0) &&
			(client_data->anymotion_enable == 0) &&
			(client_data->nomotion_enable == 0) &&
			(client_data->orientation_enable == 0) &&
			(client_data->tap_enable == 0)) {
		mutex_lock(&client_data->lock);
		err = bmi2_sensor_disable(&sens_list, 1,
						&client_data->device);
		mutex_unlock(&client_data->lock);
		PDEBUG("acc_enable %ld", op_mode);
	} else if (op_mode == 1) {
		mutex_lock(&client_data->lock);
		err = bmi2_sensor_enable(&sens_list, 1, &client_data->device);
		mutex_unlock(&client_data->lock);
		PDEBUG("acc_enable %ld", op_mode);
	}
	if (err) {
		PERR("failed");
		return err;
	}
	mutex_lock(&client_data->lock);
	client_data->pw.acc_pm = op_mode;
	mutex_unlock(&client_data->lock);

	return count;
}

/**
 * gyro_enable_show - sysfs callback which tells whether gyroscope is
 * enabled or disabled.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyro_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	u8 gyro_enable = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = bmi2_get_regs(BMI2_PWR_CTRL_ADDR, &gyro_enable, 1,
							&client_data->device);
	gyro_enable = (gyro_enable & 0x02) >> 1;
	if (err) {
		PERR("read failed");
		return err;
	}

	return snprintf(buf, 96, "%d\n", gyro_enable);
}

/**
 * gyro_enable_store - sysfs callback which enables or disables the
 * gyroscope.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyro_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long op_mode;
	u8 sens_list = BMI2_GYRO;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;
	if (op_mode == 0) {
		mutex_lock(&client_data->lock);
		err = bmi2_sensor_disable(&sens_list, 1, &client_data->device);
		mutex_unlock(&client_data->lock);
		PDEBUG("gyro_enable %ld", op_mode);
	} else if (op_mode == 1) {
		mutex_lock(&client_data->lock);
		err = bmi2_sensor_enable(&sens_list, 1, &client_data->device);
		mutex_unlock(&client_data->lock);
		PDEBUG("gyro_enable %ld", op_mode);
	} else {
		err = -EINVAL;
	}
	if (err) {
		PERR("failed");
		return err;
	}
	mutex_lock(&client_data->lock);
	client_data->pw.gyro_pm = op_mode;
	mutex_unlock(&client_data->lock);

	return count;
}

/**
 * acc_value_show - sysfs read callback which gives the
 * raw accelerometer value from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_value_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sensor_data sensor_data;

	sensor_data.type = BMI2_ACCEL;
	mutex_lock(&client_data->lock);
	err = bmi2_get_sensor_data(&sensor_data, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err < 0)
		return err;

	return snprintf(buf, 48, "%hd %hd %hd\n",
			sensor_data.sens_data.acc.x,
			sensor_data.sens_data.acc.y,
			sensor_data.sens_data.acc.z);
}

/**
 * gyro_value_show - sysfs read callback which gives the
 * raw gyroscope value from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyro_value_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sensor_data sensor_data;

	sensor_data.type = BMI2_GYRO;
	mutex_lock(&client_data->lock);
	err = bmi2_get_sensor_data(&sensor_data, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err < 0)
		return err;

	return snprintf(buf, 48, "%hd %hd %hd\n",
			sensor_data.sens_data.gyr.x,
			sensor_data.sens_data.gyr.y,
			sensor_data.sens_data.gyr.z);
}

/**
 * acc_range_show - sysfs read callback which gives the
 * accelerometer range which is set in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_range_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;
	u8 range = 0;

	mutex_lock(&client_data->lock);
	config.type = BMI2_ACCEL;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read failed");
		return err;
	}
	switch(config.cfg.acc.range){
		case 0:
			range = 0x03;
			break;
		case 1:
			range = 0x05;
			break;
		case 2:
			range = 0x08;
			break;
		case 3:
			range = 0x0C;
			break;
		default:
			range = 0xFF;
	}

	//return snprintf(buf, 16, "%d\n", config.cfg.acc.range);
	return snprintf(buf, 16, "%d\n", range);
}

/**
 * acc_range_store - sysfs write callback which sets the
 * accelerometer range to be set in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_range_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long acc_range;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = kstrtoul(buf, 10, &acc_range);
	if (err)
		return err;
	switch(acc_range){
		case 0x03:
			acc_range = 0;
			break;
		case 0x05:
			acc_range = 1;
			break;
		case 0x08:
			acc_range = 2;
			break;
		case 0x0C:
			acc_range = 3;
			break;
		default:
			PERR("out of acc range");
			return -EIO;
	}
	mutex_lock(&client_data->lock);
	config.type = BMI2_ACCEL;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.acc.range = (u8)(acc_range);
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("failed");
		return -EIO;
	}

	return count;
}

/**
 * acc_odr_show - sysfs read callback which gives the
 * accelerometer output data rate of the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_odr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	mutex_lock(&client_data->lock);
	config.type = BMI2_ACCEL;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	if (err) {
		PERR("read failed");
		mutex_unlock(&client_data->lock);
		return err;
	}
	client_data->acc_odr = config.cfg.acc.odr;
	mutex_unlock(&client_data->lock);

	return snprintf(buf, 16, "%d\n", client_data->acc_odr);
}

/**
 * acc_odr_store - sysfs write callback which sets the
 * accelerometer output data rate in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_odr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	u8 acc_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = kstrtou8(buf, 10, &acc_odr);
	if (err) {
		PERR("ODR set failed");
		return err;
	}
	mutex_lock(&client_data->lock);
	config.type = BMI2_ACCEL;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.acc.odr = acc_odr;
	if (acc_odr < BMI2_ACC_ODR_12_5HZ) {
		config.cfg.acc.filter_perf = BMI2_POWER_OPT_MODE;
		config.cfg.acc.bwp = BMI2_ACC_RES_AVG16;
	} else {
		config.cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
		config.cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
	}
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	if (err) {
		PERR("ODR set failed");
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	client_data->acc_odr = acc_odr;
	mutex_unlock(&client_data->lock);
	return count;
}

/**
 * gyro_range_show - sysfs read callback which gives the
 * gyroscope range of the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyro_range_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	mutex_lock(&client_data->lock);
	config.type = BMI2_GYRO;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read failed");
		return err;
	}

	return snprintf(buf, 16, "%d\n", config.cfg.gyr.range);
}

/**
 * gyro_range_store - sysfs write callback which sets the
 * gyroscope range in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyro_range_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long gyro_range;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = kstrtoul(buf, 10, &gyro_range);

	if (err) {
		PERR("gyro range failed");
		return err;
	}
	mutex_lock(&client_data->lock);
	config.type = BMI2_GYRO;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.gyr.range = (u8)(gyro_range);
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("failed");
		return -EIO;
	}

	return count;
}

/**
 * gyro_odr_show - sysfs read callback which gives the
 * gyroscope output data rate of the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyro_odr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	mutex_lock(&client_data->lock);
	config.type = BMI2_GYRO;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	if (err) {
		PERR("read failed");
		mutex_unlock(&client_data->lock);
		return err;
	}
	client_data->gyro_odr = config.cfg.gyr.odr;
	mutex_unlock(&client_data->lock);

	return snprintf(buf, 16, "%d\n", client_data->gyro_odr);
}

/**
 * gyro_odr_store - sysfs write callback which sets the
 * gyroscope output data rate in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyro_odr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long gyro_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = kstrtoul(buf, 10, &gyro_odr);
	if (err)
		return err;
	mutex_lock(&client_data->lock);
	config.type = BMI2_GYRO;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.gyr.odr = (u8)(gyro_odr);
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	if (err) {
		PERR("failed");
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	client_data->gyro_odr = gyro_odr;
	mutex_unlock(&client_data->lock);

	return count;
}

/**
 * accel_feat_selftest_store - sysfs write callback which performs the
 * accelerometer self test in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t accel_feat_selftest_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	u8 sensor_sel = BMI2_ACCEL_SELF_TEST;
	unsigned long selftest;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &selftest);
	if (err)
		return err;
	if (selftest == 1) {
		mutex_lock(&client_data->lock);
		/* Perform accelerometer self-test */
		err = bmi2_sensor_enable(&sensor_sel, 1, &client_data->device);
		mutex_unlock(&client_data->lock);
		if (err)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	return count;
}

/**
 * accel_feat_selftest_show - sysfs read callback which gives the
 * accelerometer self test functionality in feature of the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t accel_feat_selftest_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct bmi2_sensor_data sensor_data;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	sensor_data.type = BMI2_ACCEL_SELF_TEST;
	err = bmi2_get_sensor_data(&sensor_data, 1, &client_data->device);
	if (err)
		return err;

	return snprintf(buf, 64, "X axis %d\tY axis %d\tZ axis %d\n",
			sensor_data.sens_data.accel_self_test_output.acc_x_ok,
			sensor_data.sens_data.accel_self_test_output.acc_y_ok,
			sensor_data.sens_data.accel_self_test_output.acc_z_ok);
}

/**
 * accel_selftest_show - sysfs read callback which gives the
 * accelerometer self test result of the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t accel_selftest_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 64, "Pass => 0\tFail => 1\tNot run => 2 %d\n",
			client_data->accel_selftest);
}

/**
 * accel_selftest_store - sysfs write callback which performs the
 * accelerometer self test in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t accel_selftest_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long selftest;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &selftest);
	if (err)
		return err;
	if (selftest == 1) {
		mutex_lock(&client_data->lock);
		/* Perform accelerometer self-test */
		err = bmi2_perform_accel_self_test(&client_data->device);
		if (err)
			client_data->accel_selftest = SELF_TEST_FAIL;
		else
			client_data->accel_selftest = SELF_TEST_PASS;
		mutex_unlock(&client_data->lock);
	} else {
		err = -EINVAL;
		return err;
	}

	return count;
}

/**
 * accel_foc_show - sysfs read callback which notifies the format which
 * is to be used to perform accelerometer FOC in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t accel_foc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	return snprintf(buf, 64,
		"Use echo x_axis y_axis z_axis > accel_foc\n");
}

/**
 * accel_foc_store - sysfs write callback which performs the
 * accelerometer FOC in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t accel_foc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct accel_foc_g_value g_value = {0};
	unsigned int data[4];
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err = 0;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	err = sscanf(buf, "%d %d %d %d",
		&data[0], &data[1], &data[2], &data[3]);

	if (err != 4) {
		PERR("Invalid argument");
		return -EINVAL;
	}

	g_value.x = data[0];
	g_value.y = data[1];
	g_value.z = data[2];
	g_value.sign = data[3];
	PDEBUG("g_value.x=%d, g_value.y=%d, g_value.z=%d g_value.sign=%d",
		g_value.x, g_value.y, g_value.z, g_value.sign);
	err = bmi2_perform_accel_foc(&g_value, &client_data->device);
	if (err) {
		PERR("FOC accel failed %d", err);
		return err;
	}
	PINFO("FOC Accel successfully done");
	return count;
}

/**
 * gyro_foc_show - sysfs read callback which performs the gyroscope
 * FOC in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyro_foc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err = 0;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	err = bmi2_perform_gyro_foc(&client_data->device);
	if (err) {
		PERR("FOC gyro failed %d", err);
		return err;
	}

	return snprintf(buf, 64,
		"FOC Gyro successfully done");
}

/**
 * fifo_data_frame_show - sysfs read callback which reads the fifo data
 * according to the fifo length.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t fifo_data_frame_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	u16 fifo_bytecount = 0;
	u8 reg_data[2] = {0, };

	mutex_lock(&client_data->lock);
	if (!client_data->fifo_mag_enable && !client_data->fifo_acc_enable
					&& !client_data->fifo_gyro_enable) {
		PERR("sensor not enabled for fifo\n");
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}
	err = bmi2_get_regs(BMI2_FIFO_LENGTH_0_ADDR, reg_data, 2,
					&client_data->device);
	if (err < 0) {
		PERR("read fifo_len err=%d", err);
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}
	fifo_bytecount = (reg_data[1] << 8) | reg_data[0];

	if (fifo_bytecount == 0) {
		mutex_unlock(&client_data->lock);
		return 0;
	}
	/* Since I2c interface limit on Qualcomm based chipset is 256 */
	if (fifo_bytecount > 256)
		fifo_bytecount = 256;
	err += bmi2_get_regs(BMI2_FIFO_DATA_ADDR, buf, fifo_bytecount,
					&client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read fifo err = %d", err);
		return -EINVAL;
	}
	return fifo_bytecount;
}

/**
 * acc_fifo_enable_show - sysfs read callback which shows the enable or
 * disable status of the accelerometer FIFO in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t acc_fifo_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	u8 fifo_acc_enable;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	mutex_lock(&client_data->lock);
	/* Get the fifo config */
	err = bmi2_get_regs(0x49, &fifo_acc_enable, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	fifo_acc_enable = (fifo_acc_enable & 0x40) >> 6;
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, 16, "%x\n", fifo_acc_enable);
}

/**
 * acc_fifo_enable_store - sysfs write callback enables or
 * disables the accelerometer FIFO in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t acc_fifo_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long data;
	u8 fifo_acc_enable;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	mutex_lock(&client_data->lock);
	err = kstrtoul(buf, 10, &data);
	if (err) {
		mutex_unlock(&client_data->lock);
		return err;
	}
	fifo_acc_enable = (unsigned char)(data & 0x01);
	/* Disable advance power save to use FIFO */
	if (fifo_acc_enable) {
		err = bmi2_set_adv_power_save(0, &client_data->device);
		if (err) {
			mutex_unlock(&client_data->lock);
			return -EINVAL;
		}
	}
	err = bmi2_set_fifo_config(BMI2_FIFO_ACC_EN, fifo_acc_enable,
							&client_data->device);
	if (err) {
		PERR("failed");
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	client_data->fifo_acc_enable = fifo_acc_enable;
	mutex_unlock(&client_data->lock);

	return count;
}

/**
 * gyro_fifo_enable_store - sysfs write callback enables or
 * disables the gyroscope FIFO in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyro_fifo_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long data;
	unsigned char fifo_gyro_enable;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	mutex_lock(&client_data->lock);
	err = kstrtoul(buf, 10, &data);
	if (err) {
		mutex_unlock(&client_data->lock);
		return err;
	}
	fifo_gyro_enable = (unsigned char)(data & 0x01);

	/* Disable advance power save to use FIFO */
	if (fifo_gyro_enable) {
		err = bmi2_set_adv_power_save(0, &client_data->device);
		if (err) {
			mutex_unlock(&client_data->lock);
			return -EINVAL;
		}
	}

	err = bmi2_set_fifo_config(BMI2_FIFO_GYR_EN, fifo_gyro_enable,
							&client_data->device);
	if (err) {
		PERR("failed");
		mutex_unlock(&client_data->lock);
		return -EIO;
	}
	client_data->fifo_gyro_enable = fifo_gyro_enable;
	mutex_unlock(&client_data->lock);

	return count;
}

/**
 * gyro_fifo_enable_show - sysfs read callback which shows the enable or
 * disable status of the gyroscope FIFO in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyro_fifo_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	u8 fifo_gyro_enable;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	mutex_lock(&client_data->lock);
	/* Get the fifo config */
	err = bmi2_get_regs(BMI2_FIFO_CONFIG_1_ADDR, &fifo_gyro_enable, 1,
			&client_data->device);
	mutex_unlock(&client_data->lock);
	fifo_gyro_enable = (fifo_gyro_enable & 0x80) >> 7;
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, 16, "%x\n", fifo_gyro_enable);
}


/**
 * fifo_flush_store - sysfs write callback which flushes the fifo data
 * in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t fifo_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long enable;
	/* Command for fifo flush */
	u8 fifo_flush = 0xB0;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable == 0x01) {
		mutex_lock(&client_data->lock);
		err = bmi2_set_regs(BMI2_CMD_REG_ADDR, &fifo_flush, 1,
							&client_data->device);
		mutex_unlock(&client_data->lock);
	} else {
		return -EINVAL;
	}
	if (err) {
		PERR("write failed");
		return -EIO;
	}
	return count;
}

/**
 * aps_enable_show - sysfs callback which tells whether the advanced power
 * save mode is enabled or disabled.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t aps_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	u8 aps_status = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	/* Get the status of advance power save mode */
	err = bmi2_get_adv_power_save(&aps_status, &client_data->device);

	if (err) {
		PERR("read failed");
		return err;
	}

	/* Maximum number of bytes used to prevent overflow */
	return snprintf(buf, 96, "%d\n", aps_status);
}

/**
 * aps_enable_store - sysfs callback which enables or disables the
 * advanced power save mode.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t aps_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	u8 aps_status;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = kstrtou8(buf, 10, &aps_status);
	if (err) {
		PERR("set advance power mode failed");
		return err;
	}

	if (aps_status) {

		/* Enable advance power save mode */
		err = bmi2_set_adv_power_save(BMI2_ENABLE,
							&client_data->device);
	} else {

		/* Disable advance power save mode */
		err = bmi2_set_adv_power_save(BMI2_DISABLE,
							&client_data->device);
	}

	if (err)
		return -EINVAL;
	return count;
}

/**
 * load_config_stream_show - sysfs read callback which gives the loaded
 * config stream in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t load_config_stream_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 48, "config stream %s\n",
					client_data->config_stream_name);
}

/**
 * bmi2xy_initialize_interrupt_settings - Initialize the interrupt settings of
 * the sensor.
 *
 * @client_data: Instance of client data structure.
 *
 * Return: Status of the function.
 * * 0			- OK
 * * Negative value	- Failed
 */
static int bmi2xy_initialize_interrupt_settings(
					struct bmi2xy_client_data *client_data)
{
	int err = 0;
	/* Input enable and open drain */
	u8 int_enable = 0x0A;
	/* Permanent latched */
	u8 latch_enable = 0x01;
	/* Map all feature interrupts */
	u8 int1_map = 0xff;

	mutex_lock(&client_data->lock);
	err = bmi2_set_regs(BMI2_INT1_MAP_FEAT_ADDR, &int1_map, 1,
			&client_data->device);
	bmi2xy_i2c_delay_us(MS_TO_US(10));
	err += bmi2_set_regs(BMI2_INT1_IO_CTRL_ADDR, &int_enable, 1,
			&client_data->device);
	bmi2xy_i2c_delay_us(MS_TO_US(10));
	err += bmi2_set_regs(BMI2_INT_LATCH_ADDR, &latch_enable, 1,
			&client_data->device);
	mutex_unlock(&client_data->lock);
	bmi2xy_i2c_delay_us(MS_TO_US(10));

	if (err)
		PERR("map and enable interrupt1 failed err=%d", err);

	return err;
}

/**
 * bmi2xy_init_fifo_config - Initializes the fifo configuration of the sensor.
 *
 * @client_data: Instance of client data structure.
 *
 * Return: Status of the function.
 * * 0	-	OK
 * * Negative value	-	Failed
 */
static int bmi2xy_init_fifo_config(struct bmi2xy_client_data *client_data)
{
	int err = 0;
	u8 reg_data;

	mutex_lock(&client_data->lock);
	err = bmi2_get_regs(BMI2_FIFO_CONFIG_0_ADDR, &reg_data, 1,
							&client_data->device);
	/* Enable fifo time */
	reg_data |= 0x02;
	err = bmi2_set_regs(BMI2_FIFO_CONFIG_0_ADDR, &reg_data, 1,
							&client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("enable fifo header failed err=%d", err);
		return err;
	}
	mutex_lock(&client_data->lock);
	err = bmi2_get_regs(BMI2_FIFO_CONFIG_1_ADDR, &reg_data, 1,
							&client_data->device);
	/* Enable fifo header */
	reg_data |= 0x10;
	err = bmi2_set_regs(BMI2_FIFO_CONFIG_1_ADDR, &reg_data, 1,
							&client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("enable fifo header failed err=%d", err);
		return err;
	}

	return 0;
}

/**
 * bmi2xy_update_config_stream - Loads the config stream in the sensor.
 *
 * @client_data: Instance of client data structure.
 * @option: Option to choose different tbin images.
 *
 * Return: Status of the function.
 * * 0	-	OK
 * * Negative value	-	Failed
 */
int bmi2xy_update_config_stream(
			struct bmi2xy_client_data *client_data, int option)
{
	const struct firmware *config_entry;
	char *name;
	int err = 0;
	u8 load_status = 0;

	u8 crc_check = 0;

	switch (option) {
	case 0:
	name = "android.tbin";
	break;
	case 1:
	name = "legacy.tbin";
	break;
	case 2:
	name = "qualcomm.tbin";
	break;
	default:
	PERR("choose config stream = %d,use default ", option);
	name = "bmi2xy_config_stream";
	break;
	}
	PDEBUG("choose the config_stream %s", name);
	if ((option == 0) || (option == 1) || (option == 2)) {
		err = request_firmware(&config_entry,
			name, &client_data->acc_input->dev);
		if (err < 0) {
			PERR("Failed to get config_stream from vendor path");
			return -EINVAL;
		}
		PDEBUG("config_stream size = %zd", config_entry->size);

		client_data->device.config_file_ptr = config_entry->data;
		err = bmi220_init(&client_data->device);
		/* Delay to read the configuration load status */
		client_data->device.delay_us(20);
		/* Get the status */
		err = bmi2_get_regs(BMI2_INTERNAL_STATUS_ADDR, &load_status, 1,
							&client_data->device);
		if (!err)
			client_data->config_stream_name = name;
		if (err)
			PERR("download config stream failed: %d load status %d",
							err, load_status);
		release_firmware(config_entry);
		bmi2xy_i2c_delay_us(MS_TO_US(10));
	} else if (option == 3) {
		err = bmi220_init(&client_data->device);
		if (!err)
			name = "bmi220_config_stream";
		client_data->config_stream_name = name;

		if (err) {
			PERR("download config stream failed %d\n", err);
			return err;
		}
		bmi2xy_i2c_delay_us(MS_TO_US(200));
		err = bmi2_get_regs(BMI2_INTERNAL_STATUS_ADDR,
		&crc_check, 1, &client_data->device);
		if (err)
			PERR("reading CRC failed");
		if (crc_check != BMI2_CONFIG_LOAD_SUCCESS)
			PERR("crc check error %x", crc_check);
	}
	return err;
}

/**
 * load_config_stream_store - sysfs write callback which loads the
 * config stream in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t load_config_stream_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long choose = 0;
	int err = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_remap remapped_axis;

	err = kstrtoul(buf, 10, &choose);
	if (err)
		return err;
	PDEBUG("config_stream_choose %ld", choose);
	/* Load config file if not loaded in init stage */
	if (!client_data->config_file_loaded) {
		err = bmi2xy_update_config_stream(client_data, choose);
		if (err) {
			PERR("config_stream load error");
			return err;
		}
	}
	err = bmi2xy_initialize_interrupt_settings(client_data);
	err += bmi2xy_init_fifo_config(client_data);
	err += bmi2_get_remap_axes(&remapped_axis, &client_data->device);
	remapped_axis.x = BMI2_NEG_X;
	remapped_axis.y = BMI2_Y;
	remapped_axis.z = BMI2_NEG_Z;
	err = bmi2_set_remap_axes(&remapped_axis, &client_data->device);
	if (err) {
		PERR("Error after config stream loading %d", err);
		return err;
	}

	return count;
}

/**
 * reg_sel_show - sysfs read callback which provides the register
 * address selected.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t reg_sel_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	return snprintf(buf, 64, "reg=0X%02X, len=%d\n",
		client_data->reg_sel, client_data->reg_len);
}

/**
 * reg_sel_store - sysfs write callback which stores the register
 * address to be selected.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t reg_sel_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	mutex_lock(&client_data->lock);
	err = sscanf(buf, "%11X %11d",
		&client_data->reg_sel, &client_data->reg_len);
	if ((err != 2) || (client_data->reg_len > 128)
		|| (client_data->reg_sel > 127)) {
		PERR("Invalid argument");
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}
	mutex_unlock(&client_data->lock);
	return count;
}

/**
 * reg_val_show - sysfs read callback which shows the register
 * value which is read from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t reg_val_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 reg_data[128];
	unsigned int i;
	int pos;

	mutex_lock(&client_data->lock);
	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	if ((client_data->reg_len > 128) || (client_data->reg_sel > 127)) {
		PERR("Invalid argument");
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}

	err = bmi2_get_regs(client_data->reg_sel, reg_data,
				client_data->reg_len, &client_data->device);

	if (err < 0) {
		PERR("Reg op failed");
		mutex_unlock(&client_data->lock);
		return err;
	}
	pos = 0;
	for (i = 0; i < client_data->reg_len; ++i) {
		pos += snprintf(buf + pos, 16, "%02X", reg_data[i]);
		buf[pos++] = (i + 1) % 16 == 0 ? '\n' : ' ';
	}
	mutex_unlock(&client_data->lock);
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';
	return pos;
}

/**
 * reg_val_store - sysfs write callback which stores the register
 * value which is to be written in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t reg_val_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 reg_data[128] = {0,};
	unsigned int i, j, status, digit;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	status = 0;
	mutex_lock(&client_data->lock);
	/* Lint -save -e574 */
	for (i = j = 0; i < count && j < client_data->reg_len; ++i) {
		/* Lint -restore */
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r') {
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		PDEBUG("digit is %d", digit);
		switch (status) {
		case 2:
			++j; /* Fall thru */
		case 0:
			reg_data[j] = digit;
			status = 1;
			break;
		case 1:
			reg_data[j] = reg_data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j > client_data->reg_len)
		j = client_data->reg_len;
	else if (j < client_data->reg_len) {
		PERR("Invalid argument");
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}
	PDEBUG("Reg data read as");
	for (i = 0; i < j; ++i)
		PDEBUG("%d", reg_data[i]);
	err = client_data->device.write(client_data->device.dev_id,
		client_data->reg_sel,
		reg_data, client_data->reg_len);
	mutex_unlock(&client_data->lock);
	if (err < 0) {
		PERR("Reg op failed");
		return err;
	}
	return count;
}

/**
 * driver_version_show - sysfs read callback which provides the driver
 * version.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t driver_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 128,
		"Driver version: %s\n", DRIVER_VERSION);
}

/**
 * avail_sensor_show - sysfs read callback which provides the sensor-id
 * to the user.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t avail_sensor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u16 avail_sensor = 220;

	return snprintf(buf, 32, "%d\n", avail_sensor);
}

/**
 * execute_crt_store - sysfs write callback which triggers the
 * CRT feature in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t execute_crt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned int  data, rd_wr_len, rd_wr_len_prev;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	rd_wr_len_prev = client_data->device.read_write_len;
	err = sscanf(buf, "%u %u", &data, &rd_wr_len);
	if ((err != 2) || (data != 1) || (rd_wr_len > 510)) {
		PERR("Invalid argument");
		return -EINVAL;
	}

	client_data->device.read_write_len = rd_wr_len;
	mutex_lock(&client_data->lock);
	err = bmi2_do_crt(&client_data->device);
	mutex_unlock(&client_data->lock);
	client_data->device.read_write_len = rd_wr_len_prev;

	PDEBUG("CRT execution status %d", err);

	return count;
}


/**
 * gyr_usr_gain_show - sysfs callback which returns the gyro
 * user gain values from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 * This should be called after executing the CRT operation.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyr_usr_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	u8 gyr_usr_gain[3];
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = bmi2_get_regs(BMI2_GYR_USR_GAIN_0_ADDR, gyr_usr_gain,
				3, &client_data->device);

	if (err)
		return err;

	return snprintf(buf, 48, "x = %hd\t y = %hd\t z= %hd\n",
					gyr_usr_gain[0],
					gyr_usr_gain[1],
					gyr_usr_gain[2]);
}

/**
 * gyro_offset_comp_show - sysfs callback which returns the gyro
 * offset compensation values from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyro_offset_comp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	/* Structure to store gyroscope offset compensated data */
	struct bmi2_sens_axes_data gyr_off_comp = {0};

	/* Read the offset compensation registers */
	err =  bmi2_read_gyro_offset_comp_axes(&gyr_off_comp,
							&client_data->device);

	if (err < 0)
		return err;

	return snprintf(buf, 48, "%hd %hd %hd\n", gyr_off_comp.x,
						gyr_off_comp.y,
						gyr_off_comp.z);
}

/**
 * gyro_offset_comp_store - sysfs write callback which performs the
 * gyroscope offset compensation in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t gyro_offset_comp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long gyro_off_comp;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	u8  sensor_sel[3] = {BMI2_ACCEL, BMI2_GYRO, BMI2_GYRO_SELF_OFF};

	err = kstrtoul(buf, 10, &gyro_off_comp);

	if (err) {
		PERR("gyro offset compenstion: invalid input");
		return err;
	}

	if (gyro_off_comp) {
		/* Enable self offset correction */
		err = bmi2_sensor_enable(&sensor_sel[2], 1,
							&client_data->device);
		/* Enable gyroscope offset compensation */
		err = bmi2_set_gyro_offset_comp(BMI2_ENABLE,
							&client_data->device);
	} else {
		/* Disable self offset correction */
		err = bmi2_sensor_disable(&sensor_sel[2], 1,
							&client_data->device);
	}

	if (!err) {
		/* Enable accel and gyro */
		err = bmi2_sensor_enable(&sensor_sel[0], 2,
							&client_data->device);
	} else {
		PERR("Gyro offset compenstion failed");
		return -EIO;
	}

	return count;
}

/**
 * gyro_self_test_show - sysfs callback which performs the gyro self test
 * in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t gyro_self_test_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = bmi2_do_gyro_st(&client_data->device);

	if (err) {
		PERR("Gyro self test fail %d\n", err);
		return err;
	}

	return snprintf(buf, 64, "Gyro Self test successful\n");
}

/**
 * nvm_prog_show - sysfs read callback which gives the performs NVM
 * back up of offset compensation values for accelerometer.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t nvm_prog_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = bmi2_nvm_prog(&client_data->device);

	if (err) {
		PERR("nvm programming failed %d\n", err);
		return err;
	}

	return snprintf(buf, 64, "nvm programming successful\n");

}

/**
 * config_function_show - sysfs read callback which gives the list of
 * enabled features in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t config_function_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
		return snprintf(buf, PAGE_SIZE,
		"sig_motion0=%d\n step_detector1=%d\n step_counter2=%d\n"
		"tilt3=%d\n uphold_to_wake4=%d\n glance_detector5=%d\n"
		"wakeup6=%d\n any_motion7=%d\n"
		"orientation8=%d\n flat9=%d\n"
		"high_g10=%d\n low_g11=%d\n"
		"activity12=%d\n nomotion13=%d\n"
		"ext_sync14=%d\n",
		client_data->sigmotion_enable, client_data->stepdet_enable,
		client_data->stepcounter_enable, client_data->tilt_enable,
		client_data->uphold_to_wake, client_data->glance_enable,
		client_data->wakeup_enable, client_data->anymotion_enable,
		client_data->orientation_enable, client_data->flat_enable,
		client_data->highg_enable, client_data->lowg_enable,
		client_data->activity_enable, client_data->nomotion_enable,
		client_data->s4s_enable);
}

/**
 * config_function_store - sysfs write callback which enable or disable
 * the features in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t config_function_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int config_func = 0;
	int enable = 0;
	u8 feature = 0;
	u8 sens_list = 0;
	struct bmi2_sens_config config[2];

	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = sscanf(buf, "%11d %11d", &config_func, &enable);
	PDEBUG("config_func = %d, enable=%d", config_func, enable);
	if (ret != 2) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	if (config_func < 0 || config_func > 14)
		return -EINVAL;

	mutex_lock(&client_data->lock);
	switch (config_func) {
	case BMI2XY_SIG_MOTION_SENSOR:
		feature = BMI2_SIG_MOTION;
		client_data->sigmotion_enable = enable;
		break;
	case BMI2XY_NO_MOTION_SENSOR:
		config[0].type = BMI2_NO_MOTION;
		feature = BMI2_NO_MOTION;
		ret = bmi2_get_sensor_config(&config[0], 1,
							&client_data->device);
		/* A TYPE_STATIONARY_DETECT event is produced if the device has
		 * been stationary for at least 5 seconds with a maximal latency
		 * of 5 additional seconds. ie: it may take up anywhere from 5
		 * to 10 seconds after the device has been at rest to trigger
		 * this event.
		 */
		config[0].cfg.no_motion.duration = 0x118;
		if (enable) {
			/* Enable the x, y and z axis of nomotion */
			config[0].cfg.no_motion.select_x = BMI2_ENABLE;
			config[0].cfg.no_motion.select_y = BMI2_ENABLE;
			config[0].cfg.no_motion.select_z = BMI2_ENABLE;

		} else {
			/* Disable the x, y and z axis of nomotion */
			config[0].cfg.no_motion.select_x = BMI2_DISABLE;
			config[0].cfg.no_motion.select_y = BMI2_DISABLE;
			config[0].cfg.no_motion.select_z = BMI2_DISABLE;
		}
		ret += bmi2_set_sensor_config(&config[0], 1,
							&client_data->device);
		if (ret == 0)
			client_data->nomotion_enable =  enable;
		break;
	case BMI2XY_ORIENTATION_SENSOR:
		feature = BMI2_ORIENTATION;
		client_data->orientation_enable = enable;
		break;
	case BMI2XY_STEP_DETECTOR_SENSOR:
		feature = BMI2_STEP_DETECTOR;
		client_data->stepdet_enable = enable;
		break;
	case BMI2XY_STEP_COUNTER_SENSOR:
		feature = BMI2_STEP_COUNTER;
		client_data->stepcounter_enable = enable;
		break;
	case BMI2XY_ANY_MOTION_SENSOR:
		config[0].type = BMI2_ANY_MOTION;
		feature = BMI2_ANY_MOTION;
		ret = bmi2_get_sensor_config(&config[0], 1,
						&client_data->device);
		/* A TYPE_MOTION_DETECT event is produced if the device has been
		 * in motion for at least 5 seconds with a maximal latency of
		 * 5 additional seconds. ie: it may take up anywhere from
		 * 5 to 10 seconds after the device has been at rest to trigger
		 * this event.
		 */
		config[0].cfg.any_motion.duration = 0x118;

		if (enable) {
			/* Enable the x, y and z axis of anymotion */
			config[0].cfg.any_motion.select_x = BMI2_ENABLE;
			config[0].cfg.any_motion.select_y = BMI2_ENABLE;
			config[0].cfg.any_motion.select_z = BMI2_ENABLE;

		} else {
			/* Disable the x, y and z axis of anymotion */
			config[0].cfg.any_motion.select_x = BMI2_DISABLE;
			config[0].cfg.any_motion.select_y = BMI2_DISABLE;
			config[0].cfg.any_motion.select_z = BMI2_DISABLE;
		}
		ret += bmi2_set_sensor_config(&config[0], 1,
							&client_data->device);
		if (ret == 0)
			client_data->anymotion_enable = enable;
		break;
	case BMI2XY_ACTIVITY_SENSOR:
		feature = BMI2_STEP_ACTIVITY;
		client_data->activity_enable = enable;
		break;
	case BMI2XY_S4S:
		feature = BMI2_EXT_SENS_SYNC;
		client_data->s4s_enable = enable;
		break;
	default:
		PERR("Invalid sensor handle: %d", config_func);
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}
	sens_list = feature;
	if (enable == 1) {
		ret = bmi2_sensor_enable(&sens_list, 1, &client_data->device);
		/* Mapping activity to unused int pin 4(starts from 1) */
		if ((feature == BMI2_STEP_ACTIVITY) && (ret == 0)) {
			config[0].type = BMI2_STEP_ACTIVITY;
			ret = bmi2_get_sensor_config(&config[0], 1,
							&client_data->device);
			config[0].cfg.step_counter.out_conf_activity = 4;
			ret += bmi2_set_sensor_config(&config[0], 1,
							&client_data->device);
			}
	}
	if (enable == 0)
		ret = bmi2_sensor_disable(&sens_list, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (ret) {
		PERR("write failed %d\n", ret);
		return -EIO;
	}

	return count;
}

/**
 * feat_page_sel_show - sysfs read callback which shows the feature
 * address being selected.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t feat_page_sel_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	return snprintf(buf, 64, "reg=0X%02X, len=%d\n",
		client_data->feat_page_sel, client_data->feat_page_len);
}

/**
 * feat_page_sel_store - sysfs write callback which stores the feature
 * address to be used for reading the data.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t feat_page_sel_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	mutex_lock(&client_data->lock);
	err = sscanf(buf, "%11X %11d",
		&client_data->feat_page_sel, &client_data->feat_page_len);
	mutex_unlock(&client_data->lock);
	if (err != 2) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	return count;
}

/**
 * feat_page_val_show - sysfs read callback which provides the feature
 * data which is read from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t feat_page_val_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	u8 reg_data[128];
	unsigned int pos, i;
	u8 page;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	mutex_lock(&client_data->lock);
	page = (u8)client_data->feat_page_sel;

	err = bmi2_set_regs(BMI2_FEAT_PAGE_ADDR, &page, 1,
							&client_data->device);
	/* Get the configuration from the page */
	err += bmi2_get_regs(BMI2_FEATURES_REG_ADDR,
		reg_data, BMI2_FEAT_SIZE_IN_BYTES, &client_data->device);
	if (err < 0) {
		PERR("Reg op failed");
		mutex_unlock(&client_data->lock);
		return err;
	}
	pos = 0;
	for (i = 0; i < client_data->feat_page_len; ++i) {
		pos += snprintf(buf + pos, 16, "%02X", reg_data[i]);
		buf[pos++] = (i + 1) % 16 == 0 ? '\n' : ' ';
	}
	mutex_unlock(&client_data->lock);
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';
	return pos;
}

/**
 * feat_page_val_store - sysfs write callback which stores the feature
 * data which is to be written in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t feat_page_val_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 aps_status = 0;
	u8 reg_data[128] = {0,};
	unsigned int i, j, status, digit;
	u8 page;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	mutex_lock(&client_data->lock);
	/* Get the status of advance power save mode */
	err = bmi2_get_adv_power_save(&aps_status, &client_data->device);
	if (aps_status) {
		/* Disable advance power save mode */
		err = bmi2_set_adv_power_save(BMI2_DISABLE,
							&client_data->device);
	}
	page = (u8)client_data->feat_page_sel;

	status = 0;
	/* Lint -save -e574 */
	for (i = j = 0; i < count && j < client_data->feat_page_len; ++i) {
		/* Lint -restore */
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r') {
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		PDEBUG("digit is %d", digit);
		switch (status) {
		case 2:
			++j; /* Fall thru */
		case 0:
			reg_data[j] = digit;
			status = 1;
			break;
		case 1:
			reg_data[j] = reg_data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j > client_data->feat_page_len)
		j = client_data->feat_page_len;
	else if (j < client_data->feat_page_len) {
		PERR("Invalid argument");
		mutex_unlock(&client_data->lock);
		return -EINVAL;
	}
	PDEBUG("Reg data read as");
	for (i = 0; i < j; ++i)
		PDEBUG("%d", reg_data[i]);
	/* Switch page */
	err = bmi2_set_regs(BMI2_FEAT_PAGE_ADDR, &page, 1,
							&client_data->device);
	/* Set the configuration back to the page */
	err += bmi2_set_regs(BMI2_FEATURES_REG_ADDR,
	reg_data, BMI2_FEAT_SIZE_IN_BYTES, &client_data->device);

	if (aps_status) {
		/* Disable advance power save mode */
		err = bmi2_set_adv_power_save(BMI2_ENABLE,
							&client_data->device);
	}
	mutex_unlock(&client_data->lock);
	if (err < 0) {
		PERR("Reg op failed");
		return err;
	}
	return count;
}

/**
 * step_counter_val_show - sysfs read callback which reads and provide
 * output value of step-counter sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t step_counter_val_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err = 0;
	u32 step_counter_val = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sensor_data step_counter_data;

	step_counter_data.type = BMI2_STEP_COUNTER;
	mutex_lock(&client_data->lock);
	err = bmi2_get_sensor_data(&step_counter_data, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read failed");
		return err;
	}
	step_counter_val = step_counter_data.sens_data.step_counter_output;
	PDEBUG("val %u", step_counter_val);
	mutex_lock(&client_data->lock);
	if (client_data->err_int_trigger_num == 0) {
		client_data->step_counter_val = step_counter_val;
		PDEBUG("report val %u", client_data->step_counter_val);
		err = snprintf(buf, 96, "%u\n", client_data->step_counter_val);
		client_data->step_counter_temp = client_data->step_counter_val;
	} else {
		PDEBUG("after err report val %u",
			client_data->step_counter_val + step_counter_val);
		err = snprintf(buf, 96, "%u\n",
			client_data->step_counter_val + step_counter_val);
		client_data->step_counter_temp =
			client_data->step_counter_val + step_counter_val;
	}
	mutex_unlock(&client_data->lock);
	return err;
}

/**
 * step_counter_watermark_show - sysfs read callback which reads and
 * provide the watermark level of step-counter sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t step_counter_watermark_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	mutex_lock(&client_data->lock);
	config.type = BMI2_STEP_COUNTER;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, 32, "%d\n",
				config.cfg.step_counter.watermark_level);
}

/**
 * step_counter_watermark_store - sysfs write callback which stores the
 * watermark level of step-counter in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t step_counter_watermark_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned long step_watermark;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = kstrtoul(buf, 10, &step_watermark);
	if (err)
		return err;
	PDEBUG("watermark step_counter %ld", step_watermark);
	mutex_lock(&client_data->lock);
	config.type = BMI2_STEP_COUNTER;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.step_counter.watermark_level = (u16)step_watermark;
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("write failed");
		return err;
	}
	return count;
}

/**
 * step_counter_reset_store - sysfs write callback which resets the
 * step-counter value in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t step_counter_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned long reset_counter;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = kstrtoul(buf, 10, &reset_counter);
	if (err)
		return err;
	PDEBUG("reset_counter %ld", reset_counter);
	mutex_lock(&client_data->lock);
	config.type = BMI2_STEP_COUNTER;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.step_counter.reset_counter = (u16)reset_counter;
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	if (err) {
		PERR("write failed");
		mutex_unlock(&client_data->lock);
		return err;
	}
	client_data->step_counter_val = 0;
	client_data->step_counter_temp = 0;
	mutex_unlock(&client_data->lock);
	return count;
}

/**
 * step_buffer_size_store - sysfs callback which sets the step buffer size in
 * the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t step_buffer_size_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long step_buffer_size;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = kstrtoul(buf, 10, &step_buffer_size);

	if (err)
		return err;

	config.type = BMI2_STEP_ACTIVITY;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);

	if (err == BMI2_OK) {
		/* Get least significant byte */
		config.cfg.step_counter.step_buffer_size = step_buffer_size
								& (0xFF);
		err = bmi2_set_sensor_config(&config, 1, &client_data->device);
		if (err)
			return err;
	}
	return count;
}

/**
 * step_buffer_size_show - sysfs callback which gives the step buffer size set
 * in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t step_buffer_size_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	config.type = BMI2_STEP_ACTIVITY;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);

	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, 16, "%d\n",
				config.cfg.step_counter.step_buffer_size);
}
#if 0
/**
 * any_motion_config_show - sysfs read callback which reads the
 * any-motion configuration from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t any_motion_config_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	mutex_lock(&client_data->lock);
	config.type = BMI2_ANY_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, PAGE_SIZE, "duration =0x%x threshold= 0x%x\n",
					config.cfg.any_motion.duration,
					config.cfg.any_motion.threshold);
}

/**
 * any_motion_config_store - sysfs write callback which writes the
 * any-motion configuration in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t any_motion_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned int data[2] = {0};
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = sscanf(buf, "%11x %11x", &data[0], &data[1]);
	if (err != 2) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	mutex_lock(&client_data->lock);
	config.type = BMI2_ANY_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.any_motion.duration = (u16)data[0];
	config.cfg.any_motion.threshold = (u16)data[1];
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("write failed");
		return err;
	}
	return count;
}
#else
/////threshold/////////////
static ssize_t anymot_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	mutex_lock(&client_data->lock);
	config.type = BMI2_ANY_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n",((config.cfg.any_motion.threshold + 0x04) >> 3));//0~0x07FF ===> 0~0xFF
}

static ssize_t anymot_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned long data;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	
	data = (data << 3);//0~0xFF ===> 0~0x07FF
	
	mutex_lock(&client_data->lock);
	config.type = BMI2_ANY_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.any_motion.threshold = (u16)data;
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("write failed");
		return err;
	}
	return count;
}
//////duration////////////
static ssize_t anymot_duration_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	mutex_lock(&client_data->lock);
	config.type = BMI2_ANY_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n",config.cfg.any_motion.duration);
}
static ssize_t anymot_duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned long data;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;
	
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	
	mutex_lock(&client_data->lock);
	config.type = BMI2_ANY_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.any_motion.duration = (u16)data;
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("write failed");
		return err;
	}
	return count;
}
////////////name///////////////////
static ssize_t chip_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n","BMI220");
}
///////////temperature/////////////////////////////////////
static ssize_t temperature_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char data[2] = {0};
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	err = bmi2_get_regs(0x22,data,2,&client_data->device);
	if(err)
		return err;
	
	return snprintf(buf, PAGE_SIZE, "0x%x%x\n",data[1],data[0]);
}
#endif
/**
 * any_motion_enable_axis_store - sysfs write callback which enable or
 * disable the x,y and z axis of any-motion sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t any_motion_enable_axis_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned int data[3] = {0};
	struct bmi2_sens_config sens;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = sscanf(buf, "%11x %11x %11x", &data[0], &data[1], &data[2]);
	if (err != 3) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	/* Configure the axis for any/no-motion */
	mutex_lock(&client_data->lock);
	sens.type = BMI2_ANY_MOTION;

	err = bmi2_get_sensor_config(&sens, 1, &client_data->device);
	sens.cfg.any_motion.select_x = data[0] & 0x01;
	sens.cfg.any_motion.select_y = data[1] & 0x01;
	sens.cfg.any_motion.select_z = data[2] & 0x01;
	err = bmi2_set_sensor_config(&sens, 1, &client_data->device);
	if (err) {
		PERR("write failed");
		mutex_unlock(&client_data->lock);
		return err;
	}
	/* If user disabled all axis,then anymotion/no-motion is disabled
	 * For re-enabling anymotion/nomotion, config_function sysnode or
	 * any_no_motion_enable_axis(with atleast 1 axis enabled) can be used.
	 */
	if ((data[0] == 0) && (data[1] == 0) && (data[2] == 0))
		client_data->anymotion_enable = 0;

	mutex_unlock(&client_data->lock);

	return count;
}

/**
 * nomotion_config_show - sysfs read callback which reads the
 * no-motion configuration from the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as output.
 *
 * Return: Number of characters returned.
 */
static ssize_t no_motion_config_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	mutex_lock(&client_data->lock);
	config.type = BMI2_NO_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, PAGE_SIZE, "duration =0x%x threshold= 0x%x\n",
						config.cfg.no_motion.duration,
						config.cfg.no_motion.threshold);

}

/**
 * no_motion_config_store - sysfs write callback which writes the
 * no-motion configuration in the sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t no_motion_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned int data[2] = {0};
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);
	struct bmi2_sens_config config;

	err = sscanf(buf, "%11x %11x", &data[0], &data[1]);
	if (err != 2) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	mutex_lock(&client_data->lock);
	config.type = BMI2_NO_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.no_motion.duration = (u16)data[0];
	config.cfg.no_motion.threshold = (u16)data[1];
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	mutex_unlock(&client_data->lock);
	if (err) {
		PERR("write failed");
		return err;
	}
	return count;
}

/**
 * no_motion_enable_axis_store - sysfs write callback which enable or
 * disable the x,y and z axis of no-motion sensor.
 *
 * @dev: Device instance
 * @attr: Instance of device attribute file
 * @buf: Instance of the data buffer which serves as input.
 * @count: Number of characters in the buffer `buf`.
 *
 * Return: Number of characters used from buffer `buf`, which equals count.
 */
static ssize_t no_motion_enable_axis_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned int data[3] = {0};
	struct bmi2_sens_config sens;
	struct input_dev *input = to_input_dev(dev);
	struct bmi2xy_client_data *client_data = input_get_drvdata(input);

	err = sscanf(buf, "%11x %11x %11x", &data[0], &data[1], &data[2]);
	if (err != 3) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	/* Configure the axis for any/no-motion */
	mutex_lock(&client_data->lock);

	sens.type = BMI2_NO_MOTION;
	err = bmi2_get_sensor_config(&sens, 1, &client_data->device);
	sens.cfg.no_motion.select_x = data[0] & 0x01;
	sens.cfg.no_motion.select_y = data[1] & 0x01;
	sens.cfg.no_motion.select_z = data[2] & 0x01;

	err = bmi2_set_sensor_config(&sens, 1, &client_data->device);
	if (err) {
		PERR("write failed");
		mutex_unlock(&client_data->lock);
		return err;
	}
	/* If user disabled all axis,then anymotion/no-motion is disabled
	 * For re-enabling anymotion/nomotion, config_function sysnode or
	 * any_no_motion_enable_axis(with atleast 1 axis enabled) can be used.
	 */
	if ((data[0] == 0) && (data[1] == 0) && (data[2] == 0))
		client_data->nomotion_enable = 0;

	mutex_unlock(&client_data->lock);

	return count;
}

static DEVICE_ATTR_RO(chip_id);
static DEVICE_ATTR_RW(enable);
static DEVICE_ATTR_RO(acc_value);
static DEVICE_ATTR_RW(acc_range);
static DEVICE_ATTR_RW(acc_odr);
static DEVICE_ATTR_RW(gyro_enable);
static DEVICE_ATTR_RO(gyro_value);
static DEVICE_ATTR_RW(gyro_range);
static DEVICE_ATTR_RW(gyro_odr);
static DEVICE_ATTR_RW(accel_selftest);
static DEVICE_ATTR_RO(avail_sensor);
static DEVICE_ATTR_RW(load_config_stream);
static DEVICE_ATTR_RW(aps_enable);
static DEVICE_ATTR_RW(reg_sel);
static DEVICE_ATTR_RW(reg_val);
static DEVICE_ATTR_RO(driver_version);
static DEVICE_ATTR_RW(accel_foc);
static DEVICE_ATTR_RO(gyro_foc);
static DEVICE_ATTR_WO(fifo_flush);
static DEVICE_ATTR_RO(fifo_data_frame);
static DEVICE_ATTR_RW(acc_fifo_enable);
static DEVICE_ATTR_RW(gyro_fifo_enable);
static DEVICE_ATTR_WO(soft_reset);
static DEVICE_ATTR_RW(accel_feat_selftest);
static DEVICE_ATTR_RW(feat_page_sel);
static DEVICE_ATTR_RW(feat_page_val);
static DEVICE_ATTR_RW(config_function);
static DEVICE_ATTR_RO(nvm_prog);
static DEVICE_ATTR_RO(gyro_self_test);
static DEVICE_ATTR_RW(step_buffer_size);
static DEVICE_ATTR_RW(gyro_offset_comp);
#if 0
static DEVICE_ATTR_RW(any_motion_config);
#else
static DEVICE_ATTR_RW(anymot_threshold);
static DEVICE_ATTR_RW(anymot_duration);
static DEVICE_ATTR_RO(chip_name);
static DEVICE_ATTR_RO(temperature);
#endif
static DEVICE_ATTR_WO(any_motion_enable_axis);
static DEVICE_ATTR_RO(step_counter_val);
static DEVICE_ATTR_RW(step_counter_watermark);
static DEVICE_ATTR_WO(step_counter_reset);
static DEVICE_ATTR_RO(gyr_usr_gain);
static DEVICE_ATTR_WO(execute_crt);
static DEVICE_ATTR_RW(no_motion_config);
static DEVICE_ATTR_WO(no_motion_enable_axis);


static struct attribute *bmi2xy_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_enable.attr,
	&dev_attr_acc_value.attr,
	&dev_attr_acc_range.attr,
	&dev_attr_acc_odr.attr,
	&dev_attr_acc_fifo_enable.attr,
	&dev_attr_gyro_fifo_enable.attr,
	&dev_attr_gyro_enable.attr,
	&dev_attr_gyro_value.attr,
	&dev_attr_gyro_range.attr,
	&dev_attr_gyro_odr.attr,
	&dev_attr_accel_selftest.attr,
	&dev_attr_accel_feat_selftest.attr,
	&dev_attr_avail_sensor.attr,
	&dev_attr_driver_version.attr,
	&dev_attr_load_config_stream.attr,
	&dev_attr_aps_enable.attr,
	&dev_attr_reg_sel.attr,
	&dev_attr_reg_val.attr,
	&dev_attr_fifo_flush.attr,
	&dev_attr_fifo_data_frame.attr,
	&dev_attr_accel_foc.attr,
	&dev_attr_gyro_foc.attr,
	&dev_attr_soft_reset.attr,
	&dev_attr_feat_page_sel.attr,
	&dev_attr_feat_page_val.attr,
	&dev_attr_config_function.attr,
	&dev_attr_nvm_prog.attr,
	&dev_attr_gyro_self_test.attr,
	&dev_attr_gyro_offset_comp.attr,
	&dev_attr_step_counter_val.attr,
	&dev_attr_step_counter_watermark.attr,
	&dev_attr_step_counter_reset.attr,
#if 0
	&dev_attr_any_motion_config.attr,
#else
	&dev_attr_anymot_threshold.attr,
	&dev_attr_anymot_duration.attr,
	&dev_attr_chip_name.attr,
	&dev_attr_temperature.attr,
#endif
	&dev_attr_any_motion_enable_axis.attr,
	&dev_attr_step_buffer_size.attr,
	&dev_attr_gyr_usr_gain.attr,
	&dev_attr_execute_crt.attr,
	&dev_attr_no_motion_config.attr,
	&dev_attr_no_motion_enable_axis.attr,
	NULL
};

static struct attribute_group bmi2xy_attribute_group = {
	.attrs = bmi2xy_attributes
};

#if defined(BMI2XY_ENABLE_INT1) || defined(BMI2XY_ENABLE_INT2)
/**
 *  bmi2xy_feat_function_handle - Reports the interrupt events to HAL.
 *  @client_data : Pointer to client data structure.
 *  @status : Interrupt status information.
 */
static void bmi2xy_feat_function_handle(
			struct bmi2xy_client_data *client_data, u8 status)
{
	input_event(client_data->feat_input, EV_MSC, REL_FEAT_STATUS,
		(u32)(status));
	PDEBUG("%x", (u32)(status));
	input_sync(client_data->feat_input);
	PINFO("Interrupt occurred %x", (u32)status);
}

/**
 *  bmi2xy_irq_work_func - Bottom half handler for feature interrupts.
 *  @work : Work data for the workqueue handler.
 */
static void bmi2xy_irq_work_func(struct work_struct *work)
{
	struct bmi2xy_client_data *client_data = container_of(work,
		struct bmi2xy_client_data, irq_work);
	unsigned char int_status[2] = {0, 0};
	int err = 0;
	int in_suspend_copy;
#ifdef ANY_NO_MOTION_WORKAROUND
	u8 feature = 0;
#endif

	in_suspend_copy = atomic_read(&client_data->in_suspend);
	err = bmi2_get_regs(BMI2_INT_STATUS_0_ADDR, int_status, 2,
							&client_data->device);

	if (err) {
		PERR("Interrupt status read failed %d", err);
		return;
	}
	PDEBUG("int_status0 = 0x%x int_status1 =0x%x",
		int_status[0], int_status[1]);
	if (in_suspend_copy &&
		((int_status[0] & STEP_DET_OUT) == 0x02)) {
		return;
	}

	if (int_status[0]) {
		mutex_lock(&client_data->lock);
#ifdef ANY_NO_MOTION_WORKAROUND
		if ((int_status[0] & BMI2_ANY_MOT_INT_MASK) &&
					(client_data->anymotion_enable == 1)) {
			feature = BMI2_ANY_MOTION;
			err = bmi2_sensor_disable(&feature, 1,
							&client_data->device);
			if (err == 0)
				client_data->anymotion_enable = 0;
		} else {
			int_status[0] &= ~BMI2_ANY_MOT_INT_MASK;
		}

		if ((int_status[0] & BMI2_NO_MOT_INT_MASK) &&
					(client_data->nomotion_enable == 1)) {
			feature = BMI2_NO_MOTION;
			err = bmi2_sensor_disable(&feature, 1,
							&client_data->device);
			if (err == 0)
				client_data->nomotion_enable = 0;
		} else {
			int_status[0] &= ~BMI2_NO_MOT_INT_MASK;
		}
#endif
		bmi2xy_feat_function_handle(client_data, (u8)int_status[0]);
		mutex_unlock(&client_data->lock);
	}
}

/**
 *  bmi2xy_delay_sigmo_work_func - Bottom half handler for significant motion
 *  interrupt.
 *  @work : Work data for the workqueue handler.
 */
static void bmi2xy_delay_sigmo_work_func(struct work_struct *work)
{
	struct bmi2xy_client_data *client_data = container_of(work,
				struct bmi2xy_client_data, delay_work_sig.work);
	unsigned char int_status[2] = {0, 0};
	int err = 0;

	/* Read the interrupt status two register */
	err = bmi2_get_regs(BMI2_INT_STATUS_0_ADDR, int_status, 2,
							&client_data->device);
	if (err)
		return;
	PDEBUG("int_status0 = %x int_status1 =%x",
		int_status[0], int_status[1]);
	input_event(client_data->feat_input, EV_MSC, REL_FEAT_STATUS,
		(u32)(SIG_MOTION_OUT));
	PDEBUG("%x", (u32)(SIG_MOTION_OUT));
	input_sync(client_data->feat_input);
}

/**
 * bmi2xy_irq_handle - IRQ handler function.
 * @irq : Number of irq line.
 * @handle : Instance of client data.
 *
 * Return : Status of IRQ function.
 */
static irqreturn_t bmi2xy_irq_handle(int irq, void *handle)
{
	struct bmi2xy_client_data *client_data = handle;
	int in_suspend_copy;
	int err;

	PERR("In IRQ handle");
	in_suspend_copy = atomic_read(&client_data->in_suspend);
	/* For SIG_motion CTS test */
	if ((in_suspend_copy == 1) &&
		((client_data->sigmotion_enable == 1) &&
		(client_data->tilt_enable != 1) &&
		(client_data->uphold_to_wake != 1) &&
		(client_data->glance_enable != 1) &&
		(client_data->wakeup_enable != 1))) {
		wake_lock_timeout(&client_data->wakelock, HZ);
		err = schedule_delayed_work(&client_data->delay_work_sig,
			msecs_to_jiffies(50));
	} else if ((in_suspend_copy == 1) &&
		((client_data->sigmotion_enable == 1) ||
		(client_data->tilt_enable == 1) ||
		(client_data->uphold_to_wake == 1) ||
		(client_data->glance_enable == 1) ||
		(client_data->wakeup_enable == 1))) {
		wake_lock_timeout(&client_data->wakelock, HZ);
		err = schedule_work(&client_data->irq_work);
	} else
		err = schedule_work(&client_data->irq_work);

	if (err)
		PERR("Work will be scheduled later");

	return IRQ_HANDLED;
}

/**
 * bmi2xy_request_irq - Allocates interrupt resources and enables the
 * interrupt line and IRQ handling.
 *
 * @client_data: Instance of the client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int bmi2xy_request_irq(struct bmi2xy_client_data *client_data)
{
	int err = 0;

	client_data->gpio_pin = of_get_named_gpio_flags(
		client_data->dev->of_node,
		"bmi,gpio_irq", 0, NULL);
	if (!gpio_is_valid(client_data->gpio_pin)) {
		PERR("Invalid GPIO: %d\n", client_data->gpio_pin);
		return -EINVAL;
	}
	err = gpio_request_one(client_data->gpio_pin,
				GPIOF_IN, "bmi2xy_interrupt");
	if (err < 0)
		return err;
	err = gpio_direction_input(client_data->gpio_pin);
	if (err < 0)
		return err;
	client_data->IRQ = gpio_to_irq(client_data->gpio_pin);
	err = request_irq(client_data->IRQ, bmi2xy_irq_handle,
			IRQF_TRIGGER_RISING,
			SENSOR_NAME, client_data);
	if (err < 0)
		return err;
	/* Lint -save -e69 */
	INIT_WORK(&client_data->irq_work, bmi2xy_irq_work_func);
	INIT_DELAYED_WORK(&client_data->delay_work_sig,
		bmi2xy_delay_sigmo_work_func);
	/* Lint -restore */

	return err;
}
#endif

/**
 * bmi2xy_acc_input_init - Register the accelerometer input device in the
 * system.
 * @client_data : Instance of client data.
 *
 * Return : Status of the function.
 * * 0 - OK
 * * Any Negative value - Error.
 */
static int bmi2xy_acc_input_init(struct bmi2xy_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (dev == NULL)
		return -ENOMEM;

	dev->name = "bmi220_accel";
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_capability(dev, EV_MSC, REL_HW_STATUS);
	input_set_drvdata(dev, client_data);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	client_data->acc_input = dev;
	return 0;
}

/**
 * bmi2xy_acc_input_destroy - Un-register the Accelerometer input device from
 * the system.
 *
 * @client_data :Instance of client data.
 */
static void bmi2xy_acc_input_destroy(struct bmi2xy_client_data *client_data)
{
	struct input_dev *dev = client_data->acc_input;

	input_unregister_device(dev);
}

/**
 * bmi2xy_feat_input_init - Register the feature input device in the
 * system.
 * @client_data : Instance of client data.
 *
 * Return : Status of the function.
 * * 0 - OK.
 * * Any negative value - Error.
 */
static int bmi2xy_feat_input_init(struct bmi2xy_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (dev == NULL)
		return -ENOMEM;
	dev->name = "bmi220_gyro";
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_MSC, REL_FEAT_STATUS);
	input_set_drvdata(dev, client_data);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	client_data->feat_input = dev;
	return 0;
}

/**
 * bmi2xy_feat_input_destroy - Un-register the feature input device from the
 * system.
 * @client_data : Instance of client data.
 */
static void bmi2xy_feat_input_destroy(
	struct bmi2xy_client_data *client_data)
{
	struct input_dev *dev = client_data->acc_input;

	input_unregister_device(dev);
}

static void mg_config_init(struct bmi2xy_client_data *client_data)
{
	int err = 0;
	struct bmi2_sens_config config;
	
	mutex_lock(&client_data->lock);
	
	config.type = BMI2_ACCEL;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.acc.range = 0;
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	
	config.type = BMI2_ANY_MOTION;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.any_motion.duration = 0x00;
	config.cfg.any_motion.threshold = 0xF0;// 30 << 3
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	
	config.type = BMI2_GYRO;
	err = bmi2_get_sensor_config(&config, 1, &client_data->device);
	config.cfg.gyr.odr = 8;
	err += bmi2_set_sensor_config(&config, 1, &client_data->device);
	
	mutex_unlock(&client_data->lock);
	PINFO("%s:status: %d\n", __func__,err);
	return;
}

int bmi2xy_probe(struct bmi2xy_client_data *client_data, struct device *dev)
{
	int err = 0;
	struct bmi2_sens_config config;

	PINFO("function entrance");

	//err = dev_set_drvdata(dev, client_data);
	//if ((err) || (client_data == NULL))
	//	goto exit_err_clean;
	dev_set_drvdata(dev, client_data);
        if (client_data == NULL)
                goto exit_err_clean;

	client_data->dev = dev;
	client_data->device.delay_us = bmi2xy_i2c_delay_us;

	mutex_init(&client_data->lock);
	/* Accel input device init */
	err = bmi2xy_acc_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;
	/* Sysfs node creation */
	err = sysfs_create_group(&client_data->acc_input->dev.kobj,
			&bmi2xy_attribute_group);
	if (err < 0)
		goto exit_err_clean;
	err = bmi2xy_feat_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;
	bmi2xy_i2c_delay_us(MS_TO_US(10));
	client_data->tap_type = 0;
	/* Request irq and config*/
	#if defined(BMI2XY_ENABLE_INT1) || defined(BMI2XY_ENABLE_INT2)
	err = bmi2xy_request_irq(client_data);
	if (err < 0) {
		PERR("Request irq failed");
		goto exit_err_clean;
	}
	#endif
	wake_lock_init(&client_data->wakelock, WAKE_LOCK_SUSPEND, "bmi2xy");

	#ifdef BMI2XY_LOAD_CONFIG_FILE_IN_INIT
	err = bmi2xy_update_config_stream(client_data, 3);
	if (!err) {
		PINFO("Bosch Sensortec Device %s detected", SENSOR_NAME);
	} else {
		PERR("Bosch Sensortec Device not found");
		goto exit_err_clean;
	}

	client_data->config_file_loaded = 1;
	#endif
	/* Set the self test status */
	client_data->accel_selftest = SELF_TEST_NOT_RUN;
	client_data->gyro_selftest = SELF_TEST_NOT_RUN;

	mg_config_init(client_data);
	
	PINFO("sensor %s probed successfully", SENSOR_NAME);
	return 0;
exit_err_clean:
	if (err) {
		if (client_data != NULL)
			kfree(client_data);
		return err;
	}
	return err;
}

/**
 * bmi2xy_suspend - This function puts the driver and device to suspend mode.
 * @dev : Instance of the device.
 *
 * Return : Status of the suspend function.
 * * 0 - OK.
 * * Negative value : Error.
 */
int bmi2xy_suspend(struct device *dev)
{
	int err = 0;
	struct bmi2xy_client_data *client_data = dev_get_drvdata(dev);

	PINFO("suspend function entrance");
	err = enable_irq_wake(client_data->IRQ);
	atomic_set(&client_data->in_suspend, 1);

	return err;
}
/* Lint -save -e19 */
EXPORT_SYMBOL(bmi2xy_suspend);
/* Lint -restore */

/**
 * bmi2xy_resume - This function is used to bring back device from suspend mode
 * @dev : Instance of the device.
 *
 * Return : Status of the suspend function.
 * * 0 - OK.
 * * Negative value : Error.
 */
int bmi2xy_resume(struct device *dev)
{
	int err = 0;
	struct bmi2xy_client_data *client_data = dev_get_drvdata(dev);

	PINFO("resume function entrance");
	err = disable_irq_wake(client_data->IRQ);
	atomic_set(&client_data->in_suspend, 0);

	return err;
}
/* Lint -save -e19 */
EXPORT_SYMBOL(bmi2xy_resume);
/* Lint -restore */

/**
 * bmi2xy_remove - This function removes the driver from the device.
 * @dev : Instance of the device.
 *
 * Return : Status of the suspend function.
 * * 0 - OK.
 * * Negative value : Error.
 */
int bmi2xy_remove(struct device *dev)
{
	int err = 0;
	struct bmi2xy_client_data *client_data = dev_get_drvdata(dev);

	if (client_data != NULL) {
		bmi2xy_i2c_delay_us(MS_TO_US(BMI2XY_I2C_WRITE_DELAY_TIME));
		sysfs_remove_group(&client_data->acc_input->dev.kobj,
				&bmi2xy_attribute_group);
		bmi2xy_acc_input_destroy(client_data);
		bmi2xy_feat_input_destroy(client_data);
		wake_unlock(&client_data->wakelock);
		wake_lock_destroy(&client_data->wakelock);
		kfree(client_data);
	}
	return err;
}
/* Lint -save -e19 */
EXPORT_SYMBOL(bmi2xy_remove);
/* Lint -restore */
