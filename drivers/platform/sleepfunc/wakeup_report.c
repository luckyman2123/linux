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
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/suspend.h>

#include <linux/gpio/consumer.h>

#define SAMPLE_DEBOUNCE_MS	100   //200 ms

struct wakeup_report {
	struct device *dev;
	int wakeup_gpio;
	int wakeup_irq;
	int msg_id;
	struct delayed_work		wakeup_detect_work;
	struct wake_lock	sleep_wake_lock;
	struct power_supply *usb_psy;
};

static struct wakeup_report *wakeupreport;
static int wakeup_val = 0;
static int gpio_ctrl = 0;

static void wakeup_detect_work(struct work_struct *work)
{
    int val;
    odm_msg_type msg;
	const union power_supply_propval data;
	int ret = 0;
	int present = 0, online = 0;
	suspend_state_t state;

    val = gpio_get_value(wakeupreport->wakeup_gpio);
    wakeup_val = val;
	pr_info("[%s]gpio:%d,val %d\n", __func__, wakeupreport->wakeup_gpio,val);

	//add by liangdi for test
	ret = wakeupreport->usb_psy->get_property(wakeupreport->usb_psy, POWER_SUPPLY_PROP_PRESENT, &data);
	if (ret)
	{
		pr_err("get usb present error\n");
	}
	else
	{
		present = data.intval;
		pr_err("get usb present %d\n",present); 
	}

	ret = wakeupreport->usb_psy->get_property(wakeupreport->usb_psy, POWER_SUPPLY_PROP_ONLINE, &data);
	if (ret)
	{
		pr_err("get usb online error\n");
	}
	else
	{
		online = data.intval;
		pr_err("get usb online %d\n",online);
	}
	//add end

    if(val == 1) 
	{
		if(wakeupreport->msg_id != ODM_MSG_ID_GPIO_SUSPEND_SWITCH)
		{
	        msg.id = ODM_MSG_ID_GPIO_SUSPEND_SWITCH;
	        msg.val = 0;
			
			//add by liangdi for test
			wakeupreport->msg_id = ODM_MSG_ID_GPIO_SUSPEND_SWITCH;
			if(!online)
			{
				pr_err("set usb put out\n");
				power_supply_set_supply_type(wakeupreport->usb_psy,POWER_SUPPLY_TYPE_UNKNOWN);
				power_supply_set_present(wakeupreport->usb_psy, 0);
			}
			//add end
			odm_notify(&msg, sizeof(msg));
			schedule_delayed_work(&wakeupreport->wakeup_detect_work, msecs_to_jiffies(3000));//schedule 3000ms
		}
		else
		{
			state = pm_autosleep_state();
			if(state == PM_SUSPEND_ON)
			{
				pr_err("autosleep failed,try it again\n");
				pm_autosleep_set_state(PM_SUSPEND_MEM);
				schedule_delayed_work(&wakeupreport->wakeup_detect_work, msecs_to_jiffies(3000));//schedule 3000ms
			}
		}
    }else if((val == 0) && ((wakeupreport->msg_id != ODM_MSG_ID_GPIO_RESUME_SWITCH))) {
		msg.id = ODM_MSG_ID_GPIO_RESUME_SWITCH;
		msg.val = 1;
		
		//add by liangdi fot test
		wakeupreport->msg_id = ODM_MSG_ID_GPIO_RESUME_SWITCH;
		if(!online)
		{
			pr_err("set usb put in\n");
			power_supply_set_supply_type(wakeupreport->usb_psy,POWER_SUPPLY_TYPE_USB);
			power_supply_set_present(wakeupreport->usb_psy, 1);
		}
		//add end
		odm_notify(&msg, sizeof(msg));
    }
     return;
}

static irqreturn_t wakeup_irq_handler(int irq, void *dev_id)
{
	pr_info("wakeup irq %lu\n",msecs_to_jiffies(5000));
	wake_lock_timeout(&wakeupreport->sleep_wake_lock, msecs_to_jiffies(2000)); //wake up 2 seconds
	schedule_delayed_work(&wakeupreport->wakeup_detect_work, msecs_to_jiffies(SAMPLE_DEBOUNCE_MS));
	return IRQ_HANDLED;
}

static int wakeup_report_probe(struct platform_device *pdev)
{
    int rc = 0;
    enum of_gpio_flags flags;
    struct device_node *node;

	wakeupreport = kzalloc(sizeof(struct wakeup_report), GFP_KERNEL);
	if (!wakeupreport) {
		pr_err("wakeup_report_probe out of memory\n");
		return -ENOMEM;
	}

	wakeupreport->dev = &pdev->dev;
    node = wakeupreport->dev->of_node;
    if (!node){
       return 0;
    }

    if(!of_find_property(node, "gpios", NULL)){
        pr_info("not find gpios\n");
    }
    wakeupreport->wakeup_gpio = of_get_gpio_flags(node, 0, &flags);
    if (wakeupreport->wakeup_gpio < 0) { 
        pr_info("Failed to get gpio flags, error: %d\n",wakeupreport->wakeup_gpio);
        return 0;
    }
    rc = gpio_request(wakeupreport->wakeup_gpio, "wakeup_report");
    if (rc) {
        pr_info("%s: unable to request gpio %d\n", __func__, wakeupreport->wakeup_gpio);
        return rc;
    }
    rc = gpio_direction_input(wakeupreport->wakeup_gpio);
    if (rc){
        pr_info("gpio_direction_input fail %s\n", "WAKEUP");
        return rc;
    }
    wakeupreport->wakeup_irq = gpio_to_irq(wakeupreport->wakeup_gpio);
    if (wakeupreport->wakeup_irq < 0){  
        pr_info("unable to get irq number for GPIO %d,irq %d\n",wakeupreport->wakeup_gpio,wakeupreport->wakeup_irq);
        return 0;
    }
    rc = request_irq(wakeupreport->wakeup_irq, wakeup_irq_handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "WAKEUP_REPORT", NULL);
    if (rc){
        pr_info("request_irq fail: %d\n", rc);
        return rc;
    }

    //enable_irq_wake(wakeupreport->wakeup_irq);

	wake_lock_init(&wakeupreport->sleep_wake_lock,WAKE_LOCK_SUSPEND,"gpio_wakeup");

	INIT_DELAYED_WORK(&wakeupreport->wakeup_detect_work, wakeup_detect_work);
	//add by liangdi for test
	wakeupreport->msg_id = ODM_MSG_ID_GPIO_RESUME_SWITCH;//system power on
	wakeupreport->usb_psy = power_supply_get_by_name("usb");
	if (!wakeupreport->usb_psy) {
		pr_err("USB power_supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}
	//add end
 	//schedule_delayed_work(&wakeupreport->wakeup_detect_work, msecs_to_jiffies(1000));//schedule 1000ms

    pr_info("%s successful\n", __func__);
    return 0;
}

static int wakeup_report_remove(struct platform_device *pdev)
{
    int rc;

	pr_info("wakeup_report_remove \n");
    gpio_free(wakeupreport->wakeup_gpio);
	wake_lock_destroy(&wakeupreport->sleep_wake_lock);
    return 0;
}

static const struct of_device_id of_wakeup_report_match[] = {
    { .compatible = "wakeup_report", },
    { },
};

MODULE_DEVICE_TABLE(of, of_wakeup_report_match);

static int wakeup_report_suspend(struct device *dev)
{
	enable_irq_wake(wakeupreport->wakeup_irq);
	pr_info("wakeup_report_suspend\n");
    return 0;
}
static int wakeup_report_resume(struct device *dev)
{
	disable_irq_wake(wakeupreport->wakeup_irq);
    pr_info("wakeup_report_resume\n");
    return 0;
}

static const struct dev_pm_ops wakeup_report_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(wakeup_report_suspend, wakeup_report_resume)
};

static struct platform_driver wakeup_device_driver = {
    .probe          = wakeup_report_probe,
    .remove         = wakeup_report_remove,
    .driver         = {
                .name   = "wakeup_report",
                .owner  = THIS_MODULE,
                .pm = &wakeup_report_ops,
                .of_match_table = of_wakeup_report_match,
        }
};

module_platform_driver(wakeup_device_driver);

static int __init wakeup_report_init(void)
{
    return platform_driver_register(&wakeup_device_driver);
}

static void __exit wakeup_report_exit(void)
{
    platform_driver_unregister(&wakeup_device_driver);
}
module_init(wakeup_report_init);
module_exit(wakeup_report_exit);

module_param(wakeup_val, int, 0444);
MODULE_ALIAS("wakeup_report function");
MODULE_DESCRIPTION("wakeup report driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("yanglonglong");
MODULE_PARM_DESC(gpio_ctrl, "An visible int under sysfs");
