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
#include <linux/timer.h>

static int gpio = 0;
static int enable = 0;
static int w_disable_val = 0;
static int high = 1;
static int low = 0;

struct timer_list wdis_timer = {0};

struct w_disable_drvdata {
        const struct w_dsiable_platform_data *pdata;
        struct pinctrl *key_pinctrl;
        struct input_dev *input;
};

static void wdis_timer_expire(void)
{
	int val;
    odm_msg_type msg;
	
	val = gpio_get_value(gpio);
    w_disable_val = val;
	if((high ^ val == 1) || (low ^ val == 1))
	{
		pr_info( "w_disable_pin:%d,%s\n", val, val?"High":"Low");
		msg.id = ODM_MSG_ID_WDISABLE;
		high = val;
		low = val;
		msg.val = val;
		odm_notify(&msg, sizeof(msg));
	}
	
}

static irqreturn_t w_disable_irq_handler(int irq, void *dev_id)
{
	static int cnt = 0;
    pr_info("[%s]irq:%d gpio:%d\n", __func__, irq, gpio);
	if(cnt == 0){
	wdis_timer.function = wdis_timer_expire;
	wdis_timer.expires = jiffies + msecs_to_jiffies(10);
	add_timer(&wdis_timer);
	cnt = 1;
	}else{
      mod_timer(&wdis_timer, jiffies + msecs_to_jiffies(10));
	}
    return 0;
}

static int w_disable_pinctrl_configure(struct w_disable_drvdata *ddata,
                                                        bool active)
{
    struct pinctrl_state *set_state;
    int retval;

    if (active) {
        set_state = pinctrl_lookup_state(ddata->key_pinctrl,
                                                "tlmm_w_dsiable_active");
        if (IS_ERR(set_state)) {
                        dev_err(&ddata->input->dev,
                                "cannot get ts pinctrl active state\n");
                        return PTR_ERR(set_state);
                       }    
    } else {
        set_state = pinctrl_lookup_state(ddata->key_pinctrl,
                                                "tlmm_w_dsiable_suspend");
        if (IS_ERR(set_state)) {
                        dev_err(&ddata->input->dev,
                                "cannot get w_disable pinctrl sleep state\n");
                        return PTR_ERR(set_state);
                       }    
        }    
    retval = pinctrl_select_state(ddata->key_pinctrl, set_state);
    if (retval) {
                dev_err(&ddata->input->dev,
                                "cannot set ts pinctrl active state\n");
                return retval;
        }    

    return 0;
}

static int w_disable_probe(struct platform_device *pdev)
{
    int rc = 0;
    enum of_gpio_flags flags;
    struct device_node *node;
    int irq;
	
    pr_info("%s\n", __func__);
    struct device *dev = &pdev->dev;
    struct pinctrl *w_disable_pinctrl = devm_pinctrl_get(dev);
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
    rc = gpio_request(gpio, "w_disable");
    if (rc) {
	    pr_info("%s: unable to request gpio %d\n", __func__, gpio);
            return rc;
	}
    rc = gpio_direction_input(gpio);
	if (rc){
	    pr_info("gpio_direction_input fail %s\n", "W_DISABLE");
	    return rc;
	}
    irq = gpio_to_irq(gpio);
        if (irq < 0){  
	    pr_info("unable to get irq number for GPIO %d,irq %d\n",gpio,irq);
	    return 0;
        }
    rc = request_irq(irq, w_disable_irq_handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "W_DISABLE", NULL);
        if (rc){
	    pr_info("request_irq fail: %d\n", rc);
	    return rc;
	}
	
	init_timer(&wdis_timer);
	
printk(KERN_ERR "rc:%d\n",rc);
    return 0;
}

static int w_disable_remove(struct platform_device *pdev)
{
    int rc;
    int irq;

    irq = gpio_to_irq(gpio);
   
    gpio_free(gpio);

    rc = gpio_direction_output(gpio,1);
    if (!rc){
	    pr_info("gpio_direction_output fail %s\n", "W_DISABLE");
	    return rc;
	}
    free_irq(irq,NULL);
    return 0;
}

static const struct of_device_id w_disable_match[] = {
    { .compatible = "w_disable", },
    { },
};

static struct platform_driver w_disable_device_driver = {
    .probe          = w_disable_probe,
    .remove         = w_disable_remove,
    .driver         = {  
                .name   = "wdisable",
                .owner  = THIS_MODULE,
                .of_match_table = of_match_ptr(w_disable_match),
        }    
};

static int __init w_disable_init(void)
{
    return platform_driver_register(&w_disable_device_driver);
}

static void __exit w_disable_exit(void)
{
    platform_driver_unregister(&w_disable_device_driver);
}
module_init(w_disable_init);
module_exit(w_disable_exit);

static int irq_set_enable(const char *val, struct kernel_param *kp)
{
    int irq;
    irq = gpio_to_irq(gpio);
    switch (val[0]) {
	if (!val) val = "0";
	    case 'y': case 'Y': case '1':
		    *(int *)kp->arg = 1;
		    pr_info("%s: 1\n",__func__);
	 	    enable_irq(irq);
		    return 0;
	    case 'n': case 'N': case '0':
		    *(int *)kp->arg = 0;
		    pr_info("%s: 0 \n",__func__);
		    disable_irq(irq);
		    return 0;
	}
    return -EINVAL;
}

static int irq_get_enable(char *buffer, struct kernel_param *kp)
{
    return sprintf(buffer, "%c", (*(int *)kp->arg) ? '1' : '0');
}

module_param(w_disable_val, int, 0444);
MODULE_ALIAS("w disable function");
MODULE_DESCRIPTION("w disable driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("gaominlong");

#define module_param_call_custom(name)              \
    module_param_call(enable, irq_set_enable, irq_get_enable, &enable, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param_call_custom(enable);
MODULE_PARM_DESC(enable, "An visible int under sysfs");
