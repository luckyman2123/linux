#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h> 
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <linux/prj_macro_ctrl.h>

#define  BIT(x)   (x)
#define  PORT(x)  (x)
#define  IN   1
#define  OUT  0
#define  ENABLE   0
#define  DISABLE  1
#define  DOWN     1
#define  UP       0
#define  GPIO     1
#define  LED      0

#define  TRUE     1
#define  FALSE    0

#define  IMAX_0_0   0
#define  IMAX_3_4   1
#define  IMAX_2_4   2
#define  IMAX_1_4   3

#define  PUSH_PULL  1
#define  LIGHT_LEVEL_LOW     0
#define  LIGHT_LEVEL_HIGH    100

#define  AW9523B_ID    0x23
#define  INPUT_PORT0   0x00
#define  INPUT_PORT1   0x01
#define  OUTPUT_PORT0  0x02
#define  OUTPUT_PORT1  0x03
#define  INT_PORT0     0x06
#define  INT_PORT1     0x07
#define  ID_REG        0x10
#define  CTL_REG       0x11
#define  DIM0_P10      0x20
#define  DIM1_P11      0x21
#define  DIM2_P12      0x22
#define  DIM3_P13      0x23
#define  DIM4_P00      0x24
#define  DIM5_P01      0x25
#define  DIM6_P02      0x26
#define  DIM7_P03      0x27
#define  DIM8_P04      0x28
#define  DIM9_P05      0x29
#define  DIM10_P06     0x2A
#define  DIM11_P07     0x2B
#define  DIM12_P14     0x2C
#define  DIM13_P15     0x2D
#define  DIM14_P16     0x2E
#define  DIM15_P17     0x2F
#define  SW_RSTN       0x7F
#define  CONFIG_PORT0_DIRECT  0x04
#define  CONFIG_PORT1_DIRECT  0x05
#define  MODE_SWITCH_PORT0    0x12
#define  MODE_SWITCH_PORT1    0x13

#define  WIFI_WAKEUP  KEY_0
#define  WAKEUP_WIFI  KEY_1
#define  ANT_CLT      KEY_2
#define  WPS_KEY      KEY_3
#define  WLAN_EN      KEY_4

#define  WIFI_INDEX    0
#define  WAKEUP_INDEX  1
#define  ANT_INDEX     2
#define  WPS_INDEX     3
#define  WLAN_INDEX    4
#define  WAIT_TIMES    60

#define  WAKEUP_WIFI  _IO('A', 0x01)
#define  ANT_CTL      _IO('A', 0x02)
#define  WLAN_EN      _IO('A', 0x03)
#define  RF_LED       _IO('A', 0x04)
//#define  MSG_LED      _IO('A', 0x05)
#define  OPT_LED      _IO('A', 0x06)
#define  SIM_LED      _IO('A', 0x07)
#define  _4G_LED      _IO('A', 0x08)
#define  WPS_LED      _IO('A', 0x09)
#define  WLAN_LED     _IO('A', 0x0A)
#define  OPT1_LED     _IO('A', 0x0B)
#define  VBUS_EN      _IO('A', 0x0C)
//#define  ETH_LED      _IO('A', 0x0D)
#define  PWR_LED      _IO('A', 0x0E) 
#define  LAN_LED      _IO('A', 0x0F) 
#define  WAN_LED      _IO('A', 0x10) 
#define  NET_BLUE     _IO('A', 0x11) 
#define  NET_GREEN    _IO('A', 0x12) 
#define  NET_RED      _IO('A', 0x13) 

typedef signed char         __s8;
typedef unsigned char       __u8;
typedef signed short int    __s16;
typedef unsigned short int  __u16;
typedef signed int          __s32;
typedef unsigned int        __u32;

enum led_blink_mode {
    BLINK_MODE_1 = 1,
    BLINK_MODE_2,
    BLINK_MODE_3,
    BLINK_MODE_4,
    BLINK_MODE_5
};

enum rf_rssi {
    RF_RSSI0,
    RF_RSSI1, 
    RF_RSSI2,
    RF_RSSI3,
    RF_RSSI4,
    RF_RSSI5,
};
struct aw9523b_chip {
    int major;
    struct device *dev;
    struct i2c_client *client;
    // struct device_node *dev_node;
    unsigned int reset;
    struct mutex lock;
	int pa_power;
};
static struct wps_led {
    unsigned char enable;
    unsigned char mode;
    bool val;
};

static struct input_event old_key_stat[] = {
    {{0, 0},EV_KEY, WIFI_WAKEUP, !!UP},
    {{0, 0},EV_KEY, WAKEUP_WIFI, !!UP},
    {{0, 0},EV_KEY, ANT_CLT,     !!UP},
    {{0, 0},EV_KEY, WPS_KEY,     !!UP},
    {{0, 0},EV_KEY, WLAN_EN,     !!UP}
};
static struct input_event cur_key_stat[] = {
    {{0, 0},EV_KEY, WIFI_WAKEUP, !!UP},
    {{0, 0},EV_KEY, WAKEUP_WIFI, !!UP},
    {{0, 0},EV_KEY, ANT_CLT,     !!UP},
    {{0, 0},EV_KEY, WPS_KEY,     !!UP},
    {{0, 0},EV_KEY, WLAN_EN,     !!UP}
};

static DECLARE_WAIT_QUEUE_HEAD(key_waitq);

static struct aw9523b_chip *chip = NULL;
static struct task_struct * aw_thread = NULL;
static struct class  *aw9523b_class   = NULL;
static struct device *aw9523b_device  = NULL;

static volatile int ev_press  = 0;
static volatile int ev_enable = 0;
static struct input_dev *key_dev = NULL;
static struct fasync_struct *key_async;

static void reset_ext_gpio_dev(void)
{
    __u8 reg, data;

    reg  = SW_RSTN;
    data = 0x00;
    i2c_smbus_write_byte_data(chip->client, reg, data);
}

static __s32 get_dev_id(unsigned char *dev_id)
{
    __s32 ret;
    __u8 reg, data;

    reg = ID_REG;
    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if (ret < 0){
        dev_err(&chip->client->dev, "I2C read error \n");
        return ret;
    }

    *dev_id = (unsigned char)ret;

    return 0;
}

static __s32 set_extend_gpio_direction(void)
{
    __s32 ret;
    __u8 reg, data;

    reg = CONFIG_PORT0_DIRECT;
    if (IS_ENABLED(PROJECT_OPEN_CPU))
    {
        data = (OUT << BIT(7)) | (OUT << BIT(6)) | (OUT << BIT(5)) | (OUT << BIT(4))
             | (OUT << BIT(3)) | (OUT << BIT(2)) | (OUT << BIT(1)) | (IN << BIT(0));
    }
    if (IS_ENABLED(PRJ_SLT768))
    {
        data = (OUT << BIT(7)) | (OUT << BIT(6)) | (OUT << BIT(5)) | (OUT << BIT(4))
             | (OUT << BIT(3)) | (OUT << BIT(2)) | (OUT << BIT(1)) | (IN << BIT(0));
    }

    ret = i2c_smbus_write_byte_data(chip->client, reg, data);
    if (ret < 0){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if ((ret < 0) | (data != ret & 0xFF)){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    reg  = CONFIG_PORT1_DIRECT;
    if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
    {
        data = (OUT << BIT(7)) | (OUT << BIT(6)) | (OUT << BIT(5)) | (OUT << BIT(4))
             | (OUT << BIT(3)) | (OUT  << BIT(2)) | (OUT << BIT(1)) | (OUT << BIT(0));
    }

    if (IS_ENABLED(PRJ_SLT768))
    {
        data = (OUT << BIT(7)) | (OUT << BIT(6)) | (OUT << BIT(5)) | (OUT << BIT(4))
             | (OUT << BIT(3)) | (OUT  << BIT(2)) | (OUT << BIT(1)) | (OUT << BIT(0));
    }
	
	if (IS_ENABLED(PROJECT_OPEN_CPU))
    {
        data = (OUT << BIT(7)) | (OUT << BIT(6)) | (OUT << BIT(5)) | (OUT << BIT(4))
             | (OUT << BIT(3)) | (OUT  << BIT(2)) | (OUT << BIT(1)) | (OUT << BIT(0));
    }

    ret = i2c_smbus_write_byte_data(chip->client, reg, data);
    if (ret < 0){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if ((ret < 0) | (data != ret & 0xFF)){
        dev_err(&chip->client->dev, "I2C write error \n"); 
        return ret;
    }

    return 0;
}

static __s32 extend_gpio_int_en(void)
{
    __s32 ret;
    __u8 reg, data;

    reg  = INT_PORT0;
    if (IS_ENABLED(PROJECT_OPEN_CPU))
    {
        data = (DISABLE << BIT(7)) | (DISABLE << BIT(6)) | (DISABLE << BIT(5)) | (DISABLE << BIT(4))
             | (DISABLE << BIT(3)) | (DISABLE << BIT(2)) | (DISABLE << BIT(1)) | (ENABLE << BIT(0));
    }
    if (IS_ENABLED(PRJ_SLT768))
    {
        data = (DISABLE << BIT(7)) | (DISABLE << BIT(6)) | (DISABLE << BIT(5)) | (DISABLE << BIT(4))
             | (DISABLE << BIT(3)) | (DISABLE << BIT(2)) | (DISABLE << BIT(1)) | (ENABLE << BIT(0));
    }
    ret = i2c_smbus_write_byte_data(chip->client, reg, data);
    if (ret < 0){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if ((ret < 0) | (data != ret & 0xFF)){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    reg  = INT_PORT1;
    if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
    {
        data = (DISABLE << BIT(7)) | (DISABLE << BIT(6)) | (DISABLE << BIT(5)) | (DISABLE << BIT(4))
             | (DISABLE << BIT(3)) | (DISABLE  << BIT(2)) | (DISABLE << BIT(1)) | (DISABLE << BIT(0));
    }

    if (IS_ENABLED(PRJ_SLT768))
    {
        data = (DISABLE << BIT(7)) | (DISABLE << BIT(6)) | (DISABLE << BIT(5)) | (DISABLE << BIT(4))
             | (DISABLE << BIT(3)) | (DISABLE  << BIT(2)) | (DISABLE << BIT(1)) | (DISABLE << BIT(0));
    }
	
	 if (IS_ENABLED(PROJECT_OPEN_CPU))
    {
        data = (DISABLE << BIT(7)) | (DISABLE << BIT(6)) | (DISABLE << BIT(5)) | (DISABLE << BIT(4))
             | (DISABLE << BIT(3)) | (DISABLE  << BIT(2)) | (DISABLE << BIT(1)) | (DISABLE << BIT(0));
    }

    ret = i2c_smbus_write_byte_data(chip->client, reg, data);
    if (ret < 0){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if ((ret < 0) | (data != ret & 0xFF)){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    return 0;
}

static __s32 extend_gpio_led_mode_switch(void)
{
    __s32 ret;
    __u8 reg, data;

    reg  = MODE_SWITCH_PORT0;
    if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
    {
        data = (GPIO << BIT(7)) | (LED << BIT(6)) | (LED  << BIT(5)) | (LED << BIT(4))
             | (LED << BIT(3)) | (LED << BIT(2)) | (GPIO << BIT(1)) | (GPIO << BIT(0));
    }
    if (IS_ENABLED(PROJECT_OPEN_CPU))
    {
        data = (GPIO << BIT(7)) | (GPIO << BIT(6)) | (GPIO  << BIT(5)) | (LED << BIT(4))
             | (LED << BIT(3)) | (LED << BIT(2)) | (GPIO << BIT(1)) | (GPIO << BIT(0));
    }
    if (IS_ENABLED(PRJ_SLT768))
    {
        data = (LED << BIT(7)) | (LED << BIT(6)) | (LED  << BIT(5)) | (LED << BIT(4))
             | (LED << BIT(3)) | (LED << BIT(2)) | (GPIO << BIT(1)) | (GPIO << BIT(0));
    }
    ret = i2c_smbus_write_byte_data(chip->client, reg, data);
    if (ret < 0){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }
    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if ((ret < 0) | (data != ret & 0xFF)){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    reg  = MODE_SWITCH_PORT1;
    if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
    {
        data = (LED << BIT(7)) | (LED << BIT(6)) | (LED  << BIT(5)) | (LED << BIT(4))
            | (LED << BIT(3)) | (LED << BIT(2)) | (GPIO << BIT(1)) | (GPIO << BIT(0));
    }
    if (IS_ENABLED(PRJ_SLT768))
    {
        data = (LED << BIT(7)) | (LED << BIT(6)) | (LED  << BIT(5)) | (LED << BIT(4))
            | (LED << BIT(3)) | (LED << BIT(2)) | (LED << BIT(1)) | (GPIO << BIT(0));
    }
	if (IS_ENABLED(PROJECT_OPEN_CPU))
    {
        data = (LED << BIT(7)) | (LED << BIT(6)) | (LED  << BIT(5)) | (LED << BIT(4))
            | (LED << BIT(3)) | (LED << BIT(2)) | (LED << BIT(1)) | (GPIO << BIT(0));
    }
    ret = i2c_smbus_write_byte_data(chip->client, reg, data);
    if (ret < 0){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if ((ret < 0) | (data != ret & 0xFF)){
        dev_err(&chip->client->dev, "I2C write error \n");
        return ret;
    }

    return 0;
} 


static void set_led_drive_capability(void)
{
    __u8 reg, data;

    reg  = CTL_REG;
    data = (PUSH_PULL << 4) | (IMAX_1_4 << 0);
    i2c_smbus_write_byte_data(chip->client, reg, data);
    
    return;
}

static void set_led_all_on_off(__u8 level)
{
    __u8 reg, data;

    reg  = DIM6_P02;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);
    
    reg  = DIM7_P03;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);

    reg  = DIM8_P04;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);

    reg  = DIM9_P05;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);

    reg  = DIM10_P06;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data); 

    reg  = DIM11_P07;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);
    if (IS_ENABLED(PRJ_SLT768))
    {
        reg  = DIM1_P11;
        data = level;
        i2c_smbus_write_byte_data(chip->client, reg, data);
    }
    reg  = DIM2_P12;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);
    
    reg  = DIM3_P13;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);

    reg  = DIM12_P14;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);  

    reg  = DIM13_P15;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);  

    reg  = DIM14_P16;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data);  

    reg  = DIM15_P17;
    data = level;
    i2c_smbus_write_byte_data(chip->client, reg, data); 

    return;
}


static __s32 init_ext_dev(void)
{
    __s32 ret;
    __u32 loop;
    __u8 dev_id;

    for (loop = 0; loop < WAIT_TIMES; loop++)
    {
        ret = get_dev_id(&dev_id);
        if (AW9523B_ID == dev_id)
            break;
        mdelay(20);
    }
    if (loop >= WAIT_TIMES){
        dev_err(&chip->client->dev, "get i2c device id failed ! \n");
        return ret;
    }

    for (loop = 0; loop < WAIT_TIMES; loop++)
    {
        ret = set_extend_gpio_direction();
        if (!ret)
            break;
        mdelay(20);
    }
    if (loop >= WAIT_TIMES){
        dev_err(&chip->client->dev, "set ext_gpio aw9523b direction error !\n");
        return ret;
    }

    for (loop = 0; loop < WAIT_TIMES; loop++)
    {
        ret = extend_gpio_led_mode_switch();
        if (!ret)
            break;
        mdelay(20);
    }
    if (loop >= WAIT_TIMES){
        dev_err(&chip->client->dev, "enable ext_gpio aw9523b mode error !\n");
        return ret;
    }

    for (loop = 0; loop < WAIT_TIMES; loop++)
    {
        ret = extend_gpio_int_en();
        if (!ret)
            break;
        mdelay(20);
    }
    if (loop >= WAIT_TIMES){
        dev_err(&chip->client->dev, "enable ext_gpio aw9523b int error !\n");
        return ret;
    }

    set_led_drive_capability();
    // set_led_all_on_off(LIGHT_LEVEL_LOW);

    return 0;
}

static void power_on_self_test(void)
{
    __s32 ret;
    __u8 reg, data;

    set_extend_gpio_direction();

    reg = INPUT_PORT0;
    ret = i2c_smbus_read_byte_data(chip->client, reg);
    
    if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
        data = ret & (~(0x7C));

    if (IS_ENABLED(PRJ_SLT768))
        data = ret & (~(0x7C));
    reg = OUTPUT_PORT0;
    i2c_smbus_write_byte_data(chip->client, reg, data);

    reg = INPUT_PORT1;
    ret = i2c_smbus_read_byte_data(chip->client, reg);

    if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
        data = ret & (~(0xFC));
    if (IS_ENABLED(PRJ_SLT768))
        data = ret & (~(0xFE));

    reg = OUTPUT_PORT1;
    i2c_smbus_write_byte_data(chip->client, reg, data);
    mdelay(200);

    return;
}

static __s32 aw9523b_read_byte(__u8 reg, __u8 *data)
{
    __s32 ret;

    ret = i2c_smbus_read_byte_data(chip->client, reg);
    if (ret < 0){
        return ret;
    }
    else{
        *data = (__u8)ret;
    }

    return 0;
}

static __s32 aw9523b_write_output_bit(__u8 port, bool data, __u8 mask)
{
    __s32 ret;
    __u8 reg, val;

    if (!port)
    {
        reg = INPUT_PORT0;
        ret = i2c_smbus_read_byte_data(chip->client, reg);
        if (ret < 0){
            return ret;
        }
        else{
            val = ret;
            reg = OUTPUT_PORT0;
            if (data)
                val = val | (1 << mask);
            else
                val = val & (~(1 << mask));

            i2c_smbus_write_byte_data(chip->client, reg, val);
        }
    }
    else
    {
        reg = INPUT_PORT1;
        ret = i2c_smbus_read_byte_data(chip->client, reg);
        if (ret < 0){
            return ret;
        }
        else{
            val = ret;
            reg = OUTPUT_PORT1;
            if (data)
                val = val | (1 << mask);
            else
                val = val & (~(1 << mask));

            i2c_smbus_write_byte_data(chip->client, reg, val);
        }
    }

    return 0;
}

static irqreturn_t key_handler(int irq, void *dev_id)
{
    wake_up_interruptible(&key_waitq);
    ev_press = 1;

    return IRQ_RETVAL(IRQ_HANDLED);
}

static __s32 aw9523b_open(struct inode *inode, struct file *filp)
{
    __s32 ret;
    __u32 loop;
    __u8 dev_id;
    __u8 reg, data;

    /*
    ret = gpio_request(chip->reset, "reset");
    if (ret != 0) 
    {
        dev_err(&chip->client->dev, "Failed to gpio_request control aw9523b reset : %d \n", ret);
        gpio_free(chip->reset);
        return -EIO;
    }
    
    gpio_set_value(chip->reset, 0);
    mdelay(20);
    gpio_set_value(chip->reset, 1);
    mdelay(20);

    reset_ext_gpio_dev();
    mdelay(20);
    

    ret = init_ext_dev();
    if (ret < 0){
        dev_err(&chip->client->dev, "init_ext_dev error !\n");
        return ret;
    }*/


    reg = INPUT_PORT0;
    aw9523b_read_byte(reg, &data);
    if ((data >> 0) & 0x01)
        old_key_stat[WIFI_INDEX].value = UP;
    else
        old_key_stat[WIFI_INDEX].value = DOWN;

    if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
    {
        reg = INPUT_PORT1;
        aw9523b_read_byte(reg, &data);
        if ((data >> 1) & 0x01)
            old_key_stat[ANT_INDEX].value = UP;
        else
            old_key_stat[ANT_INDEX].value = DOWN;
    }
    reg = INPUT_PORT1;
    aw9523b_read_byte(reg, &data);
    if ((data >> 2) & 0x01)
        old_key_stat[WPS_INDEX].value = UP;
    else
        old_key_stat[WPS_INDEX].value = DOWN;

    mutex_init(&chip->lock);

    return 0;
}

static __s32 aw9523b_release(struct inode *node, struct file *filp)
{
    gpio_free(chip->reset);

    return 0;
}

static ssize_t aw9523b_read(struct file *filp, char __user *buf, size_t cnt, loff_t * offset)
{
    __u8 reg;
    __s32 data;

    mutex_lock(&chip->lock);

    copy_from_user(&reg, (void __user *)&buf[0], 1);

    data = i2c_smbus_read_byte_data(chip->client, reg);
    if (data < 0)
        return data;

    copy_to_user((void __user *)&buf[0], &data, 1);

    mutex_unlock(&chip->lock);

    return 0;
}

static ssize_t aw9523b_write(struct file *filp, const char __user *buf, size_t cnt, loff_t 
*offset)
{
    __s32 ret;
    __u8 reg, data;

    mutex_lock(&chip->lock);

    copy_from_user(&reg, (void __user *)&buf[0], 1);
    copy_from_user(&data, (void __user *)&buf[1], 1);

    ret = i2c_smbus_write_byte_data(chip->client, reg, data);

    mutex_unlock(&chip->lock);

    return ret;
}

static long aw9523b_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    __u8 reg, val, data;
    __s32 ret = 0;
    unsigned long karg = 0;

    mutex_lock(&chip->lock);

    if (copy_from_user(&karg, (void __user *)arg,
        sizeof(karg))){
        ret = -EFAULT;
    }
    val = karg & 0xFF;

    switch (cmd)
    {
        case WAKEUP_WIFI: // gpio
            aw9523b_write_output_bit(PORT(0), val, BIT(1));

            break;
        case VBUS_EN:
            aw9523b_write_output_bit(PORT(1), val, BIT(0));

            break;
        case ANT_CTL:  // gpio
            aw9523b_write_output_bit(PORT(1), val, BIT(1));

            break;
        case WLAN_EN:  // gpio
            if (IS_ENABLED(PROJECT_OPEN_CPU))
                aw9523b_write_output_bit(PORT(0), val, BIT(6));
            else
                aw9523b_write_output_bit(PORT(1), val, BIT(3));

            break;
        case RF_LED:

            if (RF_RSSI0 == val){
                if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
                {
                    i2c_smbus_write_byte_data(chip->client, DIM6_P02, LIGHT_LEVEL_LOW);
                    i2c_smbus_write_byte_data(chip->client, DIM7_P03, LIGHT_LEVEL_LOW);
                    i2c_smbus_write_byte_data(chip->client, DIM8_P04, LIGHT_LEVEL_LOW);
                    i2c_smbus_write_byte_data(chip->client, DIM9_P05, LIGHT_LEVEL_LOW);
                }
                if (IS_ENABLED(PRJ_SLT768)){
                    i2c_smbus_write_byte_data(chip->client, DIM6_P02, LIGHT_LEVEL_LOW);
                    i2c_smbus_write_byte_data(chip->client, DIM7_P03, LIGHT_LEVEL_LOW);
                    i2c_smbus_write_byte_data(chip->client, DIM8_P04, LIGHT_LEVEL_LOW);
                    i2c_smbus_write_byte_data(chip->client, DIM9_P05, LIGHT_LEVEL_LOW);
                    i2c_smbus_write_byte_data(chip->client, DIM10_P06, LIGHT_LEVEL_LOW);
                }
            }
            else if (RF_RSSI1 == val){
                i2c_smbus_write_byte_data(chip->client, DIM6_P02, LIGHT_LEVEL_HIGH);
            }
            else if (RF_RSSI2 == val){
                i2c_smbus_write_byte_data(chip->client, DIM6_P02, LIGHT_LEVEL_HIGH);
                i2c_smbus_write_byte_data(chip->client, DIM7_P03, LIGHT_LEVEL_HIGH);
            }
            else if (RF_RSSI3 == val){
                i2c_smbus_write_byte_data(chip->client, DIM6_P02, LIGHT_LEVEL_HIGH);
                i2c_smbus_write_byte_data(chip->client, DIM7_P03, LIGHT_LEVEL_HIGH);
                i2c_smbus_write_byte_data(chip->client, DIM8_P04, LIGHT_LEVEL_HIGH);
            }
            else if (RF_RSSI4 == val){
                i2c_smbus_write_byte_data(chip->client, DIM6_P02, LIGHT_LEVEL_HIGH);
                i2c_smbus_write_byte_data(chip->client, DIM7_P03, LIGHT_LEVEL_HIGH);
                i2c_smbus_write_byte_data(chip->client, DIM8_P04, LIGHT_LEVEL_HIGH);
                i2c_smbus_write_byte_data(chip->client, DIM9_P05, LIGHT_LEVEL_HIGH);
            }
            else if (RF_RSSI5 == val){
                if (IS_ENABLED(PRJ_SLT768)){
                    i2c_smbus_write_byte_data(chip->client, DIM6_P02, LIGHT_LEVEL_HIGH);
                    i2c_smbus_write_byte_data(chip->client, DIM7_P03, LIGHT_LEVEL_HIGH);
                    i2c_smbus_write_byte_data(chip->client, DIM8_P04, LIGHT_LEVEL_HIGH);
                    i2c_smbus_write_byte_data(chip->client, DIM9_P05, LIGHT_LEVEL_HIGH);
                    i2c_smbus_write_byte_data(chip->client, DIM10_P06, LIGHT_LEVEL_HIGH); 
                }
            }
            else{
                dev_err(&chip->client->dev, "Rf LED control arg is invalid ! \n");
            }

            break;

        case OPT1_LED:
            if (val)
                i2c_smbus_write_byte_data(chip->client, DIM11_P07, LIGHT_LEVEL_HIGH);
            else
                i2c_smbus_write_byte_data(chip->client, DIM11_P07, LIGHT_LEVEL_LOW);
            break;

        case SIM_LED:
            if (val)
                i2c_smbus_write_byte_data(chip->client, DIM12_P14, LIGHT_LEVEL_HIGH);
            else
                i2c_smbus_write_byte_data(chip->client, DIM12_P14, LIGHT_LEVEL_LOW);
            break;

        case _4G_LED:

            if (val)
                i2c_smbus_write_byte_data(chip->client, DIM13_P15, LIGHT_LEVEL_HIGH);
            else
                i2c_smbus_write_byte_data(chip->client, DIM13_P15, LIGHT_LEVEL_LOW);
            break;

        case WPS_LED:
            if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
            {
                if (val)
                    i2c_smbus_write_byte_data(chip->client, DIM14_P16, LIGHT_LEVEL_HIGH);
                else
                    i2c_smbus_write_byte_data(chip->client, DIM14_P16, LIGHT_LEVEL_LOW);
            }
            if (IS_ENABLED(PRJ_SLT768)){
                if (val)
                    i2c_smbus_write_byte_data(chip->client, DIM12_P14, LIGHT_LEVEL_HIGH);
                else
                    i2c_smbus_write_byte_data(chip->client, DIM12_P14, LIGHT_LEVEL_LOW);
            }
            break;

        case WLAN_LED:
            if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
            {
                if (val)
                    i2c_smbus_write_byte_data(chip->client, DIM15_P17, LIGHT_LEVEL_HIGH);
                else
                    i2c_smbus_write_byte_data(chip->client, DIM15_P17, LIGHT_LEVEL_LOW);
            }
            if (IS_ENABLED(PRJ_SLT768)){
                if (val)
                    i2c_smbus_write_byte_data(chip->client, DIM12_P14, LIGHT_LEVEL_HIGH);
                else
                    i2c_smbus_write_byte_data(chip->client, DIM12_P14, LIGHT_LEVEL_LOW);
            }
            break;

        case WAN_LED:
            if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
            {
                if (val)
                    i2c_smbus_write_byte_data(chip->client, DIM10_P06, LIGHT_LEVEL_HIGH);
                else
                    i2c_smbus_write_byte_data(chip->client, DIM10_P06, LIGHT_LEVEL_LOW);
            }
            if (IS_ENABLED(PRJ_SLT768)){
                if (val)
                    i2c_smbus_write_byte_data(chip->client, DIM2_P12, LIGHT_LEVEL_HIGH);
                else
                    i2c_smbus_write_byte_data(chip->client, DIM2_P12, LIGHT_LEVEL_LOW);
            }
            break;

        case LAN_LED:

            if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
            {
                if (val)
                    i2c_smbus_write_byte_data(chip->client, DIM2_P12, LIGHT_LEVEL_HIGH);
                else
                    i2c_smbus_write_byte_data(chip->client, DIM2_P12, LIGHT_LEVEL_LOW);
            }
            if (IS_ENABLED(PRJ_SLT768)){
                if (val)
                    i2c_smbus_write_byte_data(chip->client, DIM1_P11, LIGHT_LEVEL_HIGH);
                else
                    i2c_smbus_write_byte_data(chip->client, DIM1_P11, LIGHT_LEVEL_LOW);
            }
            break;

        case PWR_LED:

            if (val)
                i2c_smbus_write_byte_data(chip->client, DIM3_P13, LIGHT_LEVEL_HIGH);
            else
                i2c_smbus_write_byte_data(chip->client, DIM3_P13, LIGHT_LEVEL_LOW);

            break;

        case NET_BLUE:

            if (val)
                i2c_smbus_write_byte_data(chip->client, DIM13_P15, LIGHT_LEVEL_HIGH);
            else
                i2c_smbus_write_byte_data(chip->client, DIM13_P15, LIGHT_LEVEL_LOW);

            break;

        case NET_RED:

            if (val)
                i2c_smbus_write_byte_data(chip->client, DIM14_P16, LIGHT_LEVEL_HIGH);
            else
                i2c_smbus_write_byte_data(chip->client, DIM14_P16, LIGHT_LEVEL_LOW);

            break;

        case NET_GREEN:

            if (val)
                i2c_smbus_write_byte_data(chip->client, DIM15_P17, LIGHT_LEVEL_HIGH);
            else
                i2c_smbus_write_byte_data(chip->client, DIM15_P17, LIGHT_LEVEL_LOW);

            break;

        default:
            ret = -ENOIOCTLCMD;
            break;
    }
    mutex_unlock(&chip->lock);

    if (!ret)
        return 0;
    else
        return ret;
}

static int aw9523b_fasync(int fd, struct file *filp, int on)
{
    return fasync_helper(fd, filp, on, &key_async);
}

static const struct file_operations aw9523b_fops = {
    .owner   = THIS_MODULE,
    .open    = aw9523b_open,
    .release = aw9523b_release,
    .read    = aw9523b_read,
    .write   = aw9523b_write,
    .unlocked_ioctl = aw9523b_unlocked_ioctl,
    .fasync  = aw9523b_fasync,
};

static int key_input_init(void)
{
    int ret;

    key_dev = input_allocate_device();
    if (IS_ERR(key_dev)) {
        ret = PTR_ERR(key_dev);
        dev_err(&chip->client->dev, "Failed to allocate input_allocate_device: %d\n", ret);
        return ret;
    }

    key_dev->name = "cpe_key";
    key_dev->phys = "cpe_key/input1";
    key_dev->id.bustype = BUS_I2C;
    key_dev->id.vendor  = 0x0025;
    key_dev->id.product = 0x0001;
    key_dev->id.version = 0x0001;

    set_bit(EV_KEY, key_dev->evbit);

    set_bit(KEY_0, key_dev->keybit);
    set_bit(KEY_1, key_dev->keybit);
    set_bit(KEY_2, key_dev->keybit);
    set_bit(KEY_3, key_dev->keybit);    
    set_bit(KEY_4, key_dev->keybit);

    ret = input_register_device(key_dev);
    if (ret < 0){
        dev_err(&chip->client->dev, "Failed to allocate input_allocate_device: %d\n", ret);
        return ret;
    }

    return 0;
}

__s32 key_event_thread(void *arg)
{
    __s32 ret;
    __u8 reg, data;
    bool flag;

    while (1)
    {
        wait_event_interruptible(key_waitq, ev_press);
        ev_press = 0;
        flag = false;

        mdelay(10);

        reg = INPUT_PORT0;
        ret = aw9523b_read_byte(reg, &data);
        if (!ret){
            if ((data & 0x01) != (!old_key_stat[WIFI_INDEX].value)){
                flag = true;
                old_key_stat[WIFI_INDEX].value = !old_key_stat[WIFI_INDEX].value;
                if (data & 0x01)
                    input_event(key_dev, EV_KEY, WIFI_WAKEUP, UP);
                else 
                    input_event(key_dev, EV_KEY, WIFI_WAKEUP, DOWN);
            }/*
            if (((data>>1) & 0x01) != (!old_key_stat[WAKEUP_INDEX].value)){
                flag = true;
                old_key_stat[WAKEUP_INDEX].value = !old_key_stat[WAKEUP_INDEX].value;
                if ((data>>1) & 0x01)
                    input_event(key_dev, EV_KEY, WAKEUP_WIFI, UP);
                else
                    input_event(key_dev, EV_KEY, WAKEUP_WIFI, DOWN);
                printk("T ------------> old_key_stat 2 \n");
            }*/
        } 

        reg = INPUT_PORT1;
        ret = aw9523b_read_byte(reg, &data);
        if (!ret){
            if ((IS_ENABLED(PRJ_SLT758)) || (IS_ENABLED(PRJ_SLT718)))
            {
                if (((data>>1) & 0x01) != (!old_key_stat[ANT_INDEX].value)){
                    flag = true;
                    old_key_stat[ANT_INDEX].value = !old_key_stat[ANT_INDEX].value;
                    if ((data>>1) & 0x01)
                        input_event(key_dev, EV_KEY, ANT_CLT, UP);
                    else
                        input_event(key_dev, EV_KEY, ANT_CLT, DOWN);
                }
            }
            if (((data>>2) & 0x01) != (!old_key_stat[WPS_INDEX].value)){
                flag = true;
                old_key_stat[WPS_INDEX].value = !old_key_stat[WPS_INDEX].value;
                if ((data>>2) & 0x01)
                   input_event(key_dev, EV_KEY, WPS_KEY, UP);
                else
                   input_event(key_dev, EV_KEY, WPS_KEY, DOWN);
            }
            /*
            if (((data>>3) & 0x01) != (!old_key_stat[WLAN_INDEX].value)){
                flag = true;
                old_key_stat[WLAN_INDEX].value = !old_key_stat[WLAN_INDEX].value;
                if ((data>>3) & 0x01)
                    input_event(key_dev, EV_KEY, WLAN_EN, UP);
                else
                input_event(key_dev, EV_KEY, WLAN_EN, DOWN);
            } */
        }

        if (flag){
            input_sync(key_dev);
        }
        if (!ev_enable)
            break;
    }
    // kthread_stop(struct task_struct *k)
    do_exit(0);

    return 0;
}
/*
static void key_timer_fun(unsigned long data)
{
    wake_up_interruptible(&led_waitq);
    ev_blink = 1;

    mod_timer(&key_timer, jiffies+HZ / 100);
    printk("T -----> key_timer_func \n");
}*/

static ssize_t pa_mode_power_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;
	ssize_t rc = -EINVAL;
	int tmp;

	rc = kstrtoul(buf, 10, &value);
	if (rc)
		return rc;

	if(value == 1){
		gpio_direction_output(chip->pa_power, 0);
		mdelay(1);
		gpio_direction_output(chip->pa_power, 1);
		gpio_direction_output(chip->pa_power, 0);
		gpio_direction_output(chip->pa_power, 1);
		gpio_direction_output(chip->pa_power, 0);
		gpio_direction_output(chip->pa_power, 1);
		gpio_direction_output(chip->pa_power, 0);
		gpio_direction_output(chip->pa_power, 1);
	} else if(value == 0){
		rc = gpio_direction_output(chip->pa_power, 0);
		if (rc){
			pr_err("gpio_direction_output PA fail\n");
		}
	} 
	
	return count;
}

static ssize_t pa_mode_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char err;

	err = gpio_get_value(chip->pa_power);
	pr_info("pa gpio_direction_output %d\n", err);

	return snprintf(buf, 16, "%d\n", err);;
}

static DEVICE_ATTR(pa_power, 0644, pa_mode_power_show, pa_mode_power_store);

static struct attribute *aw9523b_attributes[] = {
	&dev_attr_pa_power.attr,
	NULL,
};
static const struct attribute_group aw9523b_attr_group = {
	.attrs = aw9523b_attributes,
};

static int set_gpio_init_status(void)
{
    __s32 ret;

    ret = init_ext_dev();
    if (ret < 0){
        dev_err(&chip->client->dev, "init_ext_dev error !\n");
        return ret;
    }

    if (IS_ENABLED(PRJ_SLT768)){
        aw9523b_write_output_bit(PORT(1), 0, BIT(0));
        i2c_smbus_write_byte_data(chip->client, DIM3_P13, LIGHT_LEVEL_HIGH); // set power led on
    }

    if (IS_ENABLED(PROJECT_OPEN_CPU)){
		aw9523b_write_output_bit(PORT(1), 1, BIT(0)); // enable usb vBUS_5v
        aw9523b_write_output_bit(PORT(0), 1, BIT(6)); // enable wifi DC-DC(codec CODEC_1V8_EN)
        aw9523b_write_output_bit(PORT(0), 1, BIT(5)); // enable codec CODEC_3V3_EN
        aw9523b_write_output_bit(PORT(0), 1, BIT(1)); // enable codec CODEC_PA_SHDN
    }

    return 0;
}

static int aw9523b_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    __s32 ret;
    __s32 gpio, flag;
	
    chip = kzalloc(sizeof(struct aw9523b_chip), GFP_ATOMIC);
    if (IS_ERR(chip)) {
        ret = PTR_ERR(chip);
        dev_err(&client->dev, "Failed to allocate register dev: %d\n", ret);
        return ret;
    }

    memset(chip, 0, sizeof(struct aw9523b_chip));
    chip->client  = client;
    i2c_set_clientdata(client, chip);
    chip->dev = &client->dev;
    dev_set_drvdata(chip->dev, chip);

    gpio = of_get_named_gpio_flags(chip->client->dev.of_node, "reset-gpios", 0, &flag); 
    if (!gpio_is_valid(gpio))
    {
        printk("kernel: %s-%d: invalid gpio : %d ! \n", __FUNCTION__, __LINE__, gpio);
        goto fail1;
    }
    chip->reset = gpio;
    gpio_direction_output(chip->reset, 1);
	
//start:add and modiey for pa mode ctrl，now this func don't support on this hardware version.
#if 0 
	pa_power = of_get_named_gpio_flags(chip->client->dev.of_node, "pa_mode_gpios", 0, &flag);
    if (!gpio_is_valid(pa_power))
    {
        printk("kernel: %s-%d: invalid gpio : %d ! \n", __FUNCTION__, __LINE__, pa_power);
        goto fail1;
    }
    chip->pa_power = pa_power;

    ret = gpio_request(chip->pa_power, "pa_mode_gpios");
    if (ret != 0) 
    {
        dev_err(&chip->client->dev, "Failed to gpio_request control aw9523b pa_mode_gpios : %d \n", ret);
        gpio_free(chip->pa_power);
        return -EIO;
    }
    gpio_direction_output(chip->pa_power, 0);
#endif
//end:add and modiey for pa mode ctrl，now this func don't support on this hardware version.

    chip->major = register_chrdev(0, "aw9523b", &aw9523b_fops);
    if (chip->major < 0)
    {
        printk("kernel: register_chrdev failed!\n");
        goto fail1;
    }

    aw9523b_class = class_create(THIS_MODULE, "aw9523b");
    if (IS_ERR(aw9523b_class))
    {
        printk("kernel: class_create key failed ! \n");
        goto fail2;
    }

    aw9523b_device = device_create(aw9523b_class, NULL, MKDEV(chip->major, 0), NULL, "aw9523b");
    if (IS_ERR(aw9523b_device))
    {
        printk("kernel: device_create key failed ! \n");
        goto fail3;
    }

    /* Turn on the equipment and check the LED indicator automatically */
    // power_on_self_test();
    reset_ext_gpio_dev();

	ret = sysfs_create_group(&chip->dev->kobj, &aw9523b_attr_group);
	if (ret < 0){
		kfree(chip);
		return -1;
	}
	
    ret = key_input_init();
    if (ret < 0)
    {
        printk("kernel: key_init failed ! \n");
        goto fail4;
    }

    aw_thread = kthread_run(key_event_thread, NULL, "i2c thread");
    if (IS_ERR(aw_thread))
    {
        ret = PTR_ERR(aw_thread);
        dev_err(&client->dev, "Failed to kthread_run : %d \n", ret);
        goto fail5;
    }
    ev_enable = 1;
    // init_timer(&key_timer);
    // key_timer.function = key_timer_fun;
    // add_timer(&key_timer);
    ret = request_irq(chip->client->irq, key_handler, IRQF_TRIGGER_FALLING, chip->client->name, chip->client);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "Failed to request_irq : %d \n", ret);
        goto fail6;
    }

    ret = set_gpio_init_status();
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "Failed to set_gpio_init_status : %d \n", ret);
        goto fail7;
    }

    return 0;

fail1:
    kfree(chip);
    return -1;

fail2:
    unregister_chrdev(chip->major, "aw9523b");
    kfree(chip);
    return -1;

fail3:
    class_destroy(aw9523b_class);
    unregister_chrdev(chip->major, "aw9523b");
    kfree(chip);
    return -1;

fail4:
    if (NULL != key_dev)
        input_free_device(key_dev);
    device_destroy(aw9523b_class, MKDEV(chip->major, 0));
    class_destroy(aw9523b_class);
    unregister_chrdev(chip->major, "aw9523b");
	sysfs_remove_group(&chip->dev->kobj, &aw9523b_attr_group);
    kfree(chip);
    return -1;

fail5:
    input_unregister_device(key_dev); 
    input_free_device(key_dev);
    device_destroy(aw9523b_class, MKDEV(chip->major, 0));
    class_destroy(aw9523b_class);
    unregister_chrdev(chip->major, "aw9523b");
	sysfs_remove_group(&chip->dev->kobj, &aw9523b_attr_group);
    kfree(chip);
    return -1;

fail6:
    input_unregister_device(key_dev);
    input_free_device(key_dev);
    device_destroy(aw9523b_class, MKDEV(chip->major, 0));
    class_destroy(aw9523b_class);
    unregister_chrdev(chip->major, "aw9523b");
	sysfs_remove_group(&chip->dev->kobj, &aw9523b_attr_group);
    kfree(chip);
    return -1;

fail7:
    free_irq(chip->client->irq, chip->client);
    input_unregister_device(key_dev);
    input_free_device(key_dev);
    device_destroy(aw9523b_class, MKDEV(chip->major, 0));
    class_destroy(aw9523b_class);
    unregister_chrdev(chip->major, "aw9523b");
	sysfs_remove_group(&chip->dev->kobj, &aw9523b_attr_group);
    kfree(chip);
    return -1;
}

static int aw9523b_remove(struct i2c_client *client)
{
    ev_enable = 0;
    // del_timer(&key_timer);
    
    free_irq(chip->client->irq, chip->client);
    input_unregister_device(key_dev);
    input_free_device(key_dev);
    device_destroy(aw9523b_class, MKDEV(chip->major, 0));
    class_destroy(aw9523b_class);
    unregister_chrdev(chip->major, "aw9523b");
	sysfs_remove_group(&chip->dev->kobj, &aw9523b_attr_group);
    kfree(chip);
}

static const struct i2c_device_id aw9523b_i2c_id[] = {
    { "AWINIC_AW9523B_I2C", 0 },
    { },
};
MODULE_DEVICE_TABLE(i2c, aw9523b_i2c_id);

static const struct of_device_id aw9523b_of_match[] = {
    { .compatible = "awinic,aw9523b", },
    { }
};
MODULE_DEVICE_TABLE(of, aw9523b_of_match);

static struct i2c_driver aw9523b_driver = {
    .driver = {
        .name = "aw9523b",
        .owner = THIS_MODULE,
        .of_match_table = aw9523b_of_match,
    },
    .probe = aw9523b_probe,
    .remove = aw9523b_remove,
    .id_table = aw9523b_i2c_id,
};

static int __init aw9523b_init(void)
{
    if (IS_ENABLED(PROJECT_OPEN_CPU)) {
        return i2c_add_driver(&aw9523b_driver);
    }else {
        return 0;
    }
}

static void __exit aw9523b_exit(void)
{
    i2c_del_driver(&aw9523b_driver);
}

module_init(aw9523b_init);
module_exit(aw9523b_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("i2c bus, aw9523b driver");
MODULE_AUTHOR("Shihong Li");
