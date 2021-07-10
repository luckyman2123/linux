/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
 /* *******************************Meig modification record**************************************
  *
  * Meig.yanglonglong.201708071926
  * 1.Enable this driver and disable the interrupt of this function
  *
  **********************************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>

//<--[MEIG][750][SLEEP]-yanglonglong-20170807
#define MAKE_VBUS_PLUGIN  1
#define MAKE_VBUS_PLGOUT  0

struct gpio_usbdetect *gpio_usbdat = NULL;
//end-->

struct gpio_usbdetect {
	struct platform_device	*pdev;
	struct regulator	*vin;
	struct power_supply	*usb_psy;
	int			vbus_det_irq;
};

static irqreturn_t gpio_usbdetect_vbus_irq(int irq, void *data)
{
	struct gpio_usbdetect *usb = data;
	int vbus;

	vbus = !!irq_read_line(irq);
	if (vbus)
		power_supply_set_supply_type(usb->usb_psy,
				POWER_SUPPLY_TYPE_USB);
	else
		power_supply_set_supply_type(usb->usb_psy,
				POWER_SUPPLY_TYPE_UNKNOWN);

	power_supply_set_present(usb->usb_psy, vbus);
	return IRQ_HANDLED;
}

//<--[MEIG][750][SLEEP]-yanglonglong-20170807
/****************************************************************************************** 
* Function   :gpio_usbdetect_vbus_irq_gpio
* Description:just like gpio_usbdetect_vbus_irq ,we need to simulate the real vbus 
			  plug in or out
* parameters :vbus value and usb_data
* return     :none
*******************************************************************************************/
irqreturn_t gpio_usbdetect_vbus_irq_gpio(int vbus_t, void *data)
{
	struct gpio_usbdetect *usb = data;

	if (vbus_t)
		power_supply_set_supply_type(usb->usb_psy,
				POWER_SUPPLY_TYPE_USB);
	else
		power_supply_set_supply_type(usb->usb_psy,
				POWER_SUPPLY_TYPE_UNKNOWN);

	power_supply_set_present(usb->usb_psy, vbus_t);
	return IRQ_HANDLED;
}
void gpio_usb_detect(int vbus_t)
{
	gpio_usbdetect_vbus_irq_gpio(vbus_t, gpio_usbdat);
}
EXPORT_SYMBOL(gpio_usb_detect);
//end-->

static int gpio_usbdetect_probe(struct platform_device *pdev)
{
	struct gpio_usbdetect *usb;
	struct power_supply *usb_psy;
	int rc;
	unsigned long flags;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&pdev->dev, "USB power_supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	usb = devm_kzalloc(&pdev->dev, sizeof(*usb), GFP_KERNEL);
	if (!usb)
		return -ENOMEM;

	usb->pdev = pdev;
	usb->usb_psy = usb_psy;

	if (of_get_property(pdev->dev.of_node, "vin-supply", NULL)) {
		usb->vin = devm_regulator_get(&pdev->dev, "vin");
		if (IS_ERR(usb->vin)) {
			dev_err(&pdev->dev, "Failed to get VIN regulator: %ld\n",
				PTR_ERR(usb->vin));
			return PTR_ERR(usb->vin);
		}
	}

	if (usb->vin) {
		rc = regulator_enable(usb->vin);
		if (rc) {
			dev_err(&pdev->dev, "Failed to enable VIN regulator: %d\n",
				rc);
			return rc;
		}
	}

	usb->vbus_det_irq = platform_get_irq_byname(pdev, "vbus_det_irq");
	if (usb->vbus_det_irq < 0) {
		if (usb->vin)
			regulator_disable(usb->vin);
		return usb->vbus_det_irq;
	}

	rc = devm_request_irq(&pdev->dev, usb->vbus_det_irq,
			      gpio_usbdetect_vbus_irq,
			      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			      "vbus_det_irq", usb);
	if (rc) {
		dev_err(&pdev->dev, "request for vbus_det_irq failed: %d\n",
			rc);
		if (usb->vin)
			regulator_disable(usb->vin);
		return rc;
	}

//<--[MEIG][750][SLEEP]-yanglonglong-20170807
	/*never make vbus interrupt as wakeup source*/
	//enable_irq_wake(usb->vbus_det_irq);
//end-->	
	dev_set_drvdata(&pdev->dev, usb);

	/* Read and report initial VBUS state */
	local_irq_save(flags);
//<--[MEIG][750][SLEEP]-yanglonglong-20170807	
	//gpio_usbdetect_vbus_irq(usb->vbus_det_irq, usb);
	gpio_usbdetect_vbus_irq(MAKE_VBUS_PLUGIN, usb);  //make usb port ok as default
//end-->
	local_irq_restore(flags);
//<--[MEIG][750][SLEEP]-yanglonglong-20170807		
	gpio_usbdat = usb;
	disable_irq(usb->vbus_det_irq);  //disable the vbus interrupt
//end-->
	return 0;
}

static int gpio_usbdetect_remove(struct platform_device *pdev)
{
	struct gpio_usbdetect *usb = dev_get_drvdata(&pdev->dev);
//<--[MEIG][750][SLEEP]-yanglonglong-20170807
	/*never make vbus interrupt as wakeup source*/
	//disable_irq_wake(usb->vbus_det_irq);
//end-->
	disable_irq(usb->vbus_det_irq);
	if (usb->vin)
		regulator_disable(usb->vin);

	return 0;
}

static struct of_device_id of_match_table[] = {
	{ .compatible = "qcom,gpio-usbdetect", },
	{}
};

static struct platform_driver gpio_usbdetect_driver = {
	.driver		= {
		.name	= "qcom,gpio-usbdetect",
		.of_match_table = of_match_table,
	},
	.probe		= gpio_usbdetect_probe,
	.remove		= gpio_usbdetect_remove,
};

module_driver(gpio_usbdetect_driver, platform_driver_register,
		platform_driver_unregister);

MODULE_DESCRIPTION("GPIO USB VBUS Detection driver");
MODULE_LICENSE("GPL v2");
