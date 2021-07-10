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
#include <linux/pm.h>

#include <linux/gpio/consumer.h>

#define DRIVER_NAME	"wakeup_out"

static int gpio = 0;
static int wakeup_ctrl = 0;
static int wakeup_val = 0;


static int wakeup_out_probe(struct platform_device *pdev)
{
    int rc = 0;
    enum of_gpio_flags flags;
    struct device_node *node;
    int irq;
    pr_info("%s\n", __func__);
    struct device *dev = &pdev->dev;
    struct pinctrl *wakeup_pinctrl = devm_pinctrl_get(dev);
    node = dev->of_node;
    if (!node){
       return 0;
    }
    if(!of_find_property(node, "gpios", NULL)){
        pr_info("not find gpios\n");
    }
    gpio = of_get_gpio_flags(node, 0, &flags);
    if (gpio < 0) { 
        pr_info("Failed to get gpio flags, error: %d\n",gpio);
        return 0;
    }
    rc = gpio_request(gpio, "wakeup_out");
    if (rc) {
        pr_info("%s: unable to request gpio %d\n", __func__, gpio);
        return rc;
    }
    rc = gpio_direction_output(gpio , 1);
    if (rc){
        pr_info("gpio_direction_input fail %s\n", "WAKEUP_OUT");
        return rc;
    }
    pr_info("[yll]-get wakeup_out gpio = %d\n",gpio);

    gpio_set_value(gpio, 1);

    return 0;
}

static int wakeup_out_remove(struct platform_device *pdev)
{
    int rc;
   
    gpio_free(gpio);
    gpio_set_value(gpio, 1);

    rc = gpio_direction_output(gpio,1);
    if (!rc){
        pr_info("gpio_direction_output fail %s\n", "WAKEUP_OUT");
        return rc;
    }
    return 0;
}

static const struct of_device_id of_wakeup_out_match[] = {
    { .compatible = "wakeup_out", },
    { },
};

MODULE_DEVICE_TABLE(of, of_wakeup_out_match);

static int wakeup_out_suspend(struct device *dev)
{
    gpio_set_value(gpio, 0);
    return 0;
}
static int wakeup_out_resume(struct device *dev)
{
    gpio_set_value(gpio, 1);
    return 0;
}
static const struct dev_pm_ops wakeup_out_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(wakeup_out_suspend, wakeup_out_resume)
};

static struct platform_driver wakeup_out_driver = {
    .probe = wakeup_out_probe,
    .remove = wakeup_out_remove,
    .driver = {
        .name = "wakeup_out",
        .owner = THIS_MODULE,
        .pm = &wakeup_out_ops,
        .of_match_table = of_wakeup_out_match,
    },
};
static int __init wakeup_out_init(void)
{
    return platform_driver_register(&wakeup_out_driver);
}

static void __exit wakeup_out_exit(void)
{
   platform_driver_unregister(&wakeup_out_driver);
}

module_init(wakeup_out_init);
module_exit(wakeup_out_exit);

module_platform_driver(wakeup_out_driver);

module_param(wakeup_val, int, 0444);
MODULE_ALIAS("wakeup_out function");
MODULE_DESCRIPTION("wakeup out driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("yanglonglong");

MODULE_PARM_DESC(wakeup_ctrl, "An visible int under sysfs");
