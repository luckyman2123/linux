/*!
 * @section LICENSE
 * $license_gpl$
 *
 * @filename $filename$
 * @date     2017/06/20 13:44
 * @id       $id$
 * @version  1.0.0.0
 *
 * @brief    bmi2xy SPI bus Driver
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "bmi2xy_driver.h"
#include "bs_log.h"

#define BMI2XY_MAX_BUFFER_SIZE      32

static struct spi_device *bmi2xy_spi_client;

static s8 bmi2xy_spi_write_block(uint8_t dev_addr,
	uint8_t reg_addr, uint8_t *data, uint8_t len)
{
	struct spi_device *client = bmi2xy_spi_client;
	uint8_t buffer[BMI2XY_MAX_BUFFER_SIZE + 1];
	struct spi_transfer xfer = {
		.tx_buf = buffer,
		.len = len + 1,
	};
	struct spi_message msg;

	if (len > BMI2XY_MAX_BUFFER_SIZE)
		return -EINVAL;

	buffer[0] = reg_addr&0x7F;/* write: MSB = 0 */
	memcpy(&buffer[1], data, len);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	return spi_sync(client, &msg);
}

static s8 bmi2xy_spi_read_block(uint8_t dev_addr,
	uint8_t reg_addr, uint8_t *data, uint16_t len)
{
	struct spi_device *client = bmi2xy_spi_client;
	u8 reg = reg_addr | 0x80;/* read: MSB = 1 */
	struct spi_transfer xfer[2] = {
		[0] = {
			.tx_buf = &reg,
			.len = 1,
		},
		[1] = {
			.rx_buf = data,
			.len = len,
		}
	};
	struct spi_message msg;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
	spi_message_add_tail(&xfer[1], &msg);
	return spi_sync(client, &msg);
}

int bmi2xy_spi_write_config_stream(uint8_t dev_addr,
	uint8_t reg_addr, const uint8_t *data, uint8_t len)
{
	struct spi_device *client = bmi2xy_spi_client;
	uint8_t buffer[BMI2XY_MAX_BUFFER_SIZE + 1];
	struct spi_transfer xfer = {
		.tx_buf = buffer,
		.len = len + 1,
	};
	struct spi_message msg;

	if (len > BMI2XY_MAX_BUFFER_SIZE)
		return -EINVAL;

	buffer[0] = reg_addr&0x7F;/* write: MSB = 0 */
	memcpy(&buffer[1], data, len);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	return spi_sync(client, &msg);
}

int bmi2xy_write_config_stream(uint8_t dev_addr,
	uint8_t reg_addr, const uint8_t *data, uint8_t len)
{
	int err;

	err = bmi2xy_spi_write_config_stream(dev_addr, reg_addr, data, len);
	return err;
}

static int bmi2xy_spi_probe(struct spi_device *client)
{
	int status;
	int err = 0;
	struct bmi2xy_client_data *client_data = NULL;

	if (NULL == bmi2xy_spi_client)
		bmi2xy_spi_client = client;
	else{
		PERR("This driver does not support multiple clients!\n");
		return -EBUSY;
	}
	client->bits_per_word = 8;
	status = spi_setup(client);
	if (status < 0) {
		PERR("spi_setup failed!\n");
		return status;
	}
	client_data = kzalloc(sizeof(struct bmi2xy_client_data), GFP_KERNEL);
	if (NULL == client_data) {
		PERR("no memory available");
		err = -ENOMEM;
		goto exit_err_clean;
	}

	client_data->device.bus_read = bmi2xy_spi_read_block;
	client_data->device.bus_write = bmi2xy_spi_write_block;

	return bmi2xy_probe(client_data, &client->dev);

exit_err_clean:
	if (err)
		bmi2xy_spi_client = NULL;
	return err;
}

static int bmi2xy_spi_remove(struct spi_device *client)
{
	int err = 0;

	err = bmi2xy_remove(&client->dev);
	bmi2xy_spi_client = NULL;
	return err;
}

static int bmi2xy_spi_suspend(struct spi_device *client, pm_message_t mesg)
{
	int err = 0;

	err = bmi2xy_suspend(&client->dev);
	return err;
}

static int bmi2xy_spi_resume(struct spi_device *client)
{
	int err = 0;

	err = bmi2xy_resume(&client->dev);
	return err;
}

static const struct spi_device_id bmi2xy_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(spi, bmi2xy_id);
static const struct of_device_id bmi2xy_of_match[] = {
	{ .compatible = "bmi2xy", },
	{ }
};

MODULE_DEVICE_TABLE(spi, bmi2xy_of_match);

static struct spi_driver bmi_spi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = SENSOR_NAME,
		.of_match_table = bmi2xy_of_match,
	},
	.id_table = bmi2xy_id,
	.probe    = bmi2xy_spi_probe,
	.remove   = bmi2xy_spi_remove,
	.suspend = bmi2xy_spi_suspend,
	.resume = bmi2xy_spi_resume,
};

static int __init bmi_spi_init(void)
{
	return spi_register_driver(&bmi_spi_driver);
}

static void __exit bmi_spi_exit(void)
{
	spi_unregister_driver(&bmi_spi_driver);
}


MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("BMI2XY SPI DRIVER");
MODULE_LICENSE("GPL v2");

module_init(bmi_spi_init);
module_exit(bmi_spi_exit);

