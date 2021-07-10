/* drivers/input/powerkey.c
 *
 * Copyright (C) 2016 Meig, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * Author: yaotong
 * Date:   2016-07-14
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/printk.h>
#include <odm-msg/msg.h> 
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h> //must be include,in order to use copy_from_user function
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/input.h>


#define TRUE 1
#define FALSE 0

static int gpio = 0;

static irqreturn_t powerkey_irq_handler(int irq, void *dev_id)
{
    int val;
    odm_msg_type msg;

    pr_info("[%s]irq:%d gpio:%d\n", __func__, irq, gpio);

    val = gpio_get_value(gpio);
    pr_info( "powerkey_pin:%d,%s\n", val, val?"High":"Low");
    if(val == 1) {
            	      msg.id = ODM_MSG_ID_POWEROFF;
            	      msg.val = 1;
	}else if(val == 0) {
            	      msg.id = ODM_MSG_ID_POWEROFF;
            	      msg.val = 0;
	}
     odm_notify(&msg, sizeof(msg));
     return 0;
}


static int powerkey_pinctrl_configure(struct pinctrl *p, bool active)
{
    struct pinctrl_state *set_state;
    int retval;

    if (active) {
        set_state = pinctrl_lookup_state(p, "poweroff_active");
        if (IS_ERR(set_state)) {
                        pr_info("cannot get ts pinctrl active state\n");
                        return PTR_ERR(set_state);
                       }    
    } 
	else {
        set_state = pinctrl_lookup_state(p, "poweroff_active");
        if (IS_ERR(set_state)) {
                        pr_info("cannot get poweroff pinctrl sleep state\n");
                        return PTR_ERR(set_state);
                       }    
        }    
    retval = pinctrl_select_state(p, set_state);
    if (retval) {
                pr_info("cannot set ts pinctrl active state\n");
                return retval;
        }    

    return 0;
}

static int powerkey_probe(struct platform_device *pdev)
{
    int rc = 0;
    enum of_gpio_flags flags;
    struct device_node *node;
    int irq;

    struct device *dev = &pdev->dev;
    struct pinctrl *poweroff_pinctrl = devm_pinctrl_get(dev);
    node = dev->of_node;
    if (!node)
	{
            return 0;
	};
    if(!of_find_property(node, "gpios", NULL)){
	    pr_info("not find gpios\n");
	};
    gpio = of_get_gpio_flags(node, 0, &flags);
    if (gpio < 0) { 
            pr_info("Failed to get gpio flags, error: %d\n",gpio);
            return 0;
            }
	powerkey_pinctrl_configure(poweroff_pinctrl, TRUE);
	rc = pinctrl_request_gpio(gpio);
    if (rc) {
	    pr_info("%s: unable to request gpio %d\n", __func__, gpio);
            return rc;
	}
	rc = pinctrl_gpio_direction_input(gpio);
	if (rc){
	    pr_info("gpio_direction_input fail %s\n", "poweroff");
	    return rc;
	}
    irq = gpio_to_irq(gpio);   /* get irq number */
    if (irq < 0){  
	    pr_info("unable to get irq number for GPIO %d,irq %d\n",gpio,irq);
	    return 0;
        }
	rc = request_irq(irq, powerkey_irq_handler, IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, "poweroff", NULL);
    
	if (rc){
	    pr_info("request_irq fail: %d\n", rc);
	    return rc;
	}
    printk(KERN_ERR "gpio_%d level=%d\n",gpio,gpio_get_value(gpio));
    return 0;
}

static int powerkey_remove(struct platform_device *pdev)
{
    int rc;
    int irq;
    
    irq = gpio_to_irq(gpio);
	free_irq(irq,NULL);
    gpio_free(gpio);

    rc = gpio_direction_output(gpio,1);
    if (!rc){
	    pr_info("gpio_direction_output fail %s\n", "POWEROFF");
	    return rc;
	}
    return 0;
}

static const struct of_device_id powerkey_match[]={
	{.compatible = "poweroff"},
	{},
};

static struct platform_driver powerkey_device_driver ={
	.probe = powerkey_probe,
	.remove = powerkey_remove,
	.driver = {
		.name = "poweroff",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(powerkey_match),
	}
};
static int __init powerkey_init(void)
{
	return platform_driver_register(&powerkey_device_driver);
}

static int __exit powerkey_exit(void)
{
	platform_driver_unregister(&powerkey_device_driver);
}

module_init(powerkey_init);
module_exit(powerkey_exit);

MODULE_ALIAS("powerkey function");
MODULE_DESCRIPTION("powerkey driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("yaotong");
