/* drivers/input/keyboard/s3c-keypad.c
 *
 * Driver core for Samsung SoC onboard UARTs.
 *
 * Kim Kyoungil, Copyright (c) 2006-2009 Samsung Electronics
 *      http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/slab.h>

#include <linux/io.h>
#include <mach/hardware.h>
#include <asm/delay.h>
#include <asm/irq.h>

#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-keypad.h>
#ifdef CONFIG_CPU_FREQ
#include <mach/cpu-freq-v210.h>
#endif

#include "s3c-keypad-forte.h"

#define USE_PERF_LEVEL_KEYPAD 1
#undef S3C_KEYPAD_DEBUG
//#define S3C_KEYPAD_DEBUG

#ifdef S3C_KEYPAD_DEBUG
#define DPRINTK(x...) printk("S3C-Keypad " x)
#define INPUT_REPORT_KEY(a,b,c) do {				\
		printk(KERN_ERR "%s:%d input_report_key(%x, %d, %d)\n", \
		       __func__, __LINE__, a, b, c);			\
		input_report_key(a,b,c);				\
	} while (0)
#else
#define DPRINTK(x...)		/* !!!! */
#define INPUT_REPORT_KEY	input_report_key
#endif
#define FUNC_TRACE()        printk(KERN_ERR "%s\n", __func__) ;

#define DEVICE_NAME "s3c-keypad"

#define TRUE 1
#define FALSE 0
#define	SUBJECT	"s3c_keypad.c"
#define P(format,...)\
    printk ("[ "SUBJECT " (%s,%d) ] " format "\n", __func__, __LINE__, ## __VA_ARGS__);
#define FI \
    printk ("[ "SUBJECT " (%s,%d) ] " "%s - IN" "\n", __func__, __LINE__, __func__);
#define FO \
    printk ("[ "SUBJECT " (%s,%d) ] " "%s - OUT" "\n", __func__, __LINE__, __func__);

static struct timer_list keypad_timer;
static int is_timer_on = FALSE;
static struct clk *keypad_clock;



static u32 keymask[KEYPAD_COLUMNS];
static u32 prevmask[KEYPAD_COLUMNS];

static int in_sleep = 0;

// active keyled  {{
int keyLedOn = 0;
int keyLedSigOn = 0;
static void keypad_led_onoff(int OnOff);
void keypad_led_onoff_Ext(int OnOff){ keypad_led_onoff(OnOff); }
// active keyled  }}

static void keypad_scan_activate_column(int active_col)
{
    u32     cval ;

    switch (active_col)
    {
        case -1 :   /* Clear All */
            s3c_gpio_cfgpin(GPIO_KEYSCAN6, S3C_GPIO_OUTPUT) ;
            s3c_gpio_cfgpin(GPIO_KEYSCAN7, S3C_GPIO_OUTPUT) ;
            s3c_gpio_setpin(GPIO_KEYSCAN6, GPIO_LEVEL_LOW) ;
            s3c_gpio_setpin(GPIO_KEYSCAN7, GPIO_LEVEL_LOW) ;
            writel(KEYIFCOL_CLEAR, key_base+S3C_KEYIFCOL);
            break ;

        case 6 :
            s3c_gpio_cfgpin(GPIO_KEYSCAN6, S3C_GPIO_OUTPUT) ;
            s3c_gpio_setpin(GPIO_KEYSCAN6, GPIO_LEVEL_LOW) ;
            s3c_gpio_cfgpin(GPIO_KEYSCAN7, S3C_GPIO_INPUT) ;    /* HI-Z */
            cval = KEYCOL_DMASK & 0xFFFF ; /* All port Hi-Z */
            writel(cval, key_base+S3C_KEYIFCOL) ;
            break ;

        case 7 :
            s3c_gpio_cfgpin(GPIO_KEYSCAN7, S3C_GPIO_OUTPUT) ;
            s3c_gpio_setpin(GPIO_KEYSCAN7, GPIO_LEVEL_LOW) ;
            s3c_gpio_cfgpin(GPIO_KEYSCAN6, S3C_GPIO_INPUT) ;    /* HI-Z */
            cval = KEYCOL_DMASK & 0xFFFF ; /* All port Hi-Z */
            writel(cval, key_base+S3C_KEYIFCOL) ;
             break ;

        default :
            s3c_gpio_cfgpin(GPIO_KEYSCAN6, S3C_GPIO_INPUT) ;    /* HI-Z */
            s3c_gpio_cfgpin(GPIO_KEYSCAN7, S3C_GPIO_INPUT) ;    /* HI-Z */
            cval = KEYCOL_DMASK & ~(((1<<8) | 1) << active_col) ;
            writel(cval, key_base+S3C_KEYIFCOL) ;
            break ;
    }
}

static int keypad_scan(void)
{
    u32 col,rval;

    // DPRINTK("H3C %x H2C %x \n",readl(S5PV210_GPH3CON),readl(S5PV210_GPH2CON));
    DPRINTK("keypad_scan() is called\n");
    DPRINTK("row val = %x",readl(key_base + S3C_KEYIFROW));

    for (col = 0 ; col < KEYPAD_COLUMNS ; col++)
    {
        keypad_scan_activate_column(col) ;
        udelay(KEYPAD_DELAY);

        rval = ~(readl(key_base+S3C_KEYIFROW)) & ((1<<KEYPAD_ROWS)-1) ;
        keymask[col] = rval;
    }

    keypad_scan_activate_column(-1) ;   /* All Column Low */

    return 0;
}


static void keypad_timer_handler(unsigned long data)
{
    u32 press_mask;
    u32 release_mask;
    u32 restart_timer = 0;
    int i,col;
    struct s3c_keypad *pdata = (struct s3c_keypad *)data;
    struct input_dev *dev = pdata->dev;

    keypad_scan();

    for(col=0; col < KEYPAD_COLUMNS; col++)
    {
        press_mask = ((keymask[col] ^ prevmask[col]) & keymask[col]);
        release_mask = ((keymask[col] ^ prevmask[col]) & prevmask[col]);

    #ifdef CONFIG_CPU_FREQ
    #if USE_PERF_LEVEL_KEYPAD
 //      if (press_mask || release_mask)
//            set_dvfs_target_level(LEV_400MHZ);
    #endif
    #endif
        i = col * KEYPAD_ROWS;

        while (press_mask) {
            if (press_mask & 1) {
                input_report_key(dev,pdata->keycodes[i],1);
                DPRINTK("\nkey Pressed  : key %d map %d\n",i, pdata->keycodes[i]);
                printk("\nkey Pressed  : key %d map %d\n",i, pdata->keycodes[i]);
            }
            press_mask >>= 1;
            i++;
        }

        i = col * KEYPAD_ROWS;

        while (release_mask) {
            if (release_mask & 1) {               
                input_report_key(dev,pdata->keycodes[i],0);

                DPRINTK("\nkey Released : %d  map %d\n",i,pdata->keycodes[i]);
                printk("\nkey Released : %d  map %d\n",i,pdata->keycodes[i]);
            }
            release_mask >>= 1;
            i++;
        }
        prevmask[col] = keymask[col];

        restart_timer |= keymask[col];
    }

    if (restart_timer) {
        mod_timer(&keypad_timer,jiffies + (3*HZ/100));    // 30ms
    } else {
        writel(KEYIFCON_INIT, key_base+S3C_KEYIFCON);
        is_timer_on = FALSE;
    }
}


static irqreturn_t s3c_keypad_isr(int irq, void *dev_id)
{

	printk("%s: Entering Keypad Isr \n", __func__);
	/* disable keypad interrupt and schedule for keypad timer handler */
	writel(readl(key_base+S3C_KEYIFCON) & ~(INT_F_EN|INT_R_EN), key_base+S3C_KEYIFCON);

	keypad_timer.expires = jiffies + (3*HZ/100);        // 30ms
	if ( is_timer_on == FALSE) {
		add_timer(&keypad_timer);
		is_timer_on = TRUE;
	} else {
		mod_timer(&keypad_timer,keypad_timer.expires);
	}
	/*Clear the keypad interrupt status*/
	writel(KEYIFSTSCLR_CLEAR, key_base+S3C_KEYIFSTSCLR);

	return IRQ_HANDLED;
}



static irqreturn_t s3c_keygpio_isr(int irq, void *dev_id)
{
	unsigned int key_status;
	static unsigned int prev_key_status = (1 << 6);
	struct s3c_keypad *pdata = (struct s3c_keypad *)dev_id;
	struct input_dev *dev = pdata->dev;
    #define KEY_POWER_EVT       58
    // TODO:FORTE ATLAS 26

	// Beware that we may not obtain exact key up/down status at
	// this point.
	key_status = (readl(S5PV210_GPH2DAT)) & (1 << 6);

	// If ISR is called and up/down status remains the same, we
	// must have lost one and should report that first with
	// upside/down.
	if(in_sleep)
	{
	    if (key_status == prev_key_status)
		{
    		INPUT_REPORT_KEY(dev, KEY_POWER_EVT, key_status ? 1 : 0);
		}
		in_sleep = 0;
	}

	INPUT_REPORT_KEY(dev, KEY_POWER_EVT, key_status ? 0 : 1);

	prev_key_status = key_status;
	printk(KERN_DEBUG "s3c_keygpio_isr pwr key_status =%d,\n", key_status);

	return IRQ_HANDLED;
}

extern void TSP_forced_release_forOKkey(void);
static irqreturn_t s3c_keygpio_ok_isr(int irq, void *dev_id)
{
	unsigned int key_status;
	static unsigned int prev_key_status = (1 << 5);
	struct s3c_keypad *pdata = (struct s3c_keypad *)dev_id;
	struct input_dev *dev = pdata->dev;

//	#ifdef CONFIG_CPU_FREQ
//	set_dvfs_target_level(LEV_800MHZ);
//	#endif
	// Beware that we may not obtain exact key up/down status at
	// this point.
	key_status = (readl(S5PV210_GPH3DAT)) & ((1 << 5));

	// If ISR is called and up/down status remains the same, we
	// must have lost one and should report that first with
	// upside/down.
	if(in_sleep)
	{
	if (key_status == prev_key_status)
		{
		INPUT_REPORT_KEY(dev, 50, key_status ? 1 : 0);
		}
		in_sleep = 0;
	}

	INPUT_REPORT_KEY(dev, 50, key_status ? 0 : 1);

	if(key_status)
		TSP_forced_release_forOKkey();

	prev_key_status = key_status;
        printk(KERN_DEBUG "s3c_keygpio_ok_isr key_status =%d,\n", key_status);

        return IRQ_HANDLED;
}

extern unsigned int HWREV;

static int s3c_keygpio_isr_setup(void *pdev)
{
	int ret;
#if 0
        s3c_gpio_setpull(S5PV210_GPH3(0), S3C_GPIO_PULL_UP);
	set_irq_type(IRQ_EINT(24), IRQ_TYPE_EDGE_BOTH);
	ret = request_irq(IRQ_EINT(24), s3c_keygpio_ok_isr, IRQF_SAMPLE_RANDOM,
		"key ok", (void *) pdev);
	if (ret) {
		printk("request_irq failed (IRQ_KEYPAD (key ok)) !!!\n");
		ret = -EIO;
		  return ret;
	}
#endif
    //PWR key
    s3c_gpio_setpull(S5PV210_GPH2(6), S3C_GPIO_PULL_NONE);
    set_irq_type(IRQ_EINT(22), IRQ_TYPE_EDGE_BOTH);

    // stk.lim: Add IRQF_DISABLED to eliminate any possible race
    // regarding key status
#if 0
   ret = request_irq(IRQ_EINT(22), s3c_keygpio_isr, IRQF_DISABLED
                        | IRQF_SAMPLE_RANDOM, "key gpio", (void *)pdev);

    if (ret) {
        printk("request_irq failed (IRQ_KEYPAD (gpio)) !!!\n");
        ret = -EIO;
    }
#endif
    return ret;
}

static void keypad_led_onoff(int OnOff)
{
   int slide_open = 0;

   slide_open = !((readl(S5PV210_GPH0DAT)) & (1 << 5));

   printk(KERN_ERR "\nKeypad_LED_ONOFF(Slide=%d)(OnOff=%d)\n", slide_open, OnOff);

   if(OnOff)
   {
      if(slide_open)
      {
        keyLedOn = 1; // active keyled
        //CONFIG_FORTE_HW_REV03
        s3c_gpio_cfgpin(GPIO_MAIN_KEY_LED_EN, S3C_GPIO_OUTPUT);
         gpio_set_value(GPIO_MAIN_KEY_LED_EN, GPIO_LEVEL_HIGH);
        s3c_gpio_cfgpin(GPIO_MAIN_KEY_LED_REV03_EN, S3C_GPIO_OUTPUT);
         gpio_set_value(GPIO_MAIN_KEY_LED_REV03_EN, GPIO_LEVEL_HIGH);
      }
   }
   else
   {
      // if(slide_open)
      {
        keyLedOn = 0; // active keyled
        //CONFIG_FORTE_HW_REV03
         s3c_gpio_cfgpin(GPIO_MAIN_KEY_LED_EN, S3C_GPIO_OUTPUT);
         gpio_set_value(GPIO_MAIN_KEY_LED_EN, GPIO_LEVEL_LOW);
         s3c_gpio_cfgpin(GPIO_MAIN_KEY_LED_REV03_EN, S3C_GPIO_OUTPUT);
         gpio_set_value(GPIO_MAIN_KEY_LED_REV03_EN, GPIO_LEVEL_LOW);
      }
   }
}

static irqreturn_t s3c_slide_isr(int irq, void *dev_id)
{
    unsigned int slide_is_closed = 0;
    struct s3c_keypad *pdata = (struct s3c_keypad *)dev_id;
    struct input_dev *dev = pdata->dev;

    /* 0 : open, 1 : closed */
    slide_is_closed = readl(S5PV210_GPH0DAT) & (1 << 5);
    //hojun_kim ]
    printk(KERN_INFO "\nSlide status=%x(0:open, 1:closed)\n", slide_is_closed);

    input_report_switch(dev, SW_LID, slide_is_closed);
    
    input_sync(dev); //hojun_kim

   return IRQ_HANDLED;
}

static int s3c_slidegpio_isr_setup(void *pdev)
{
    int ret;
    int slide_is_closed;
    struct s3c_keypad *pkeypad = (struct s3c_keypad *)pdev;

    s3c_gpio_cfgpin(S5PV210_GPH0(5), S3C_GPIO_SFN(0));
    s3c_gpio_setpull(S5PV210_GPH0(5), S3C_GPIO_PULL_NONE);
    set_irq_type(IRQ_EINT5, IRQ_TYPE_EDGE_BOTH);

    ret = can_request_irq(IRQ_EINT5, IRQF_DISABLED);
    printk(KERN_INFO "\n can_request_irq=%d\n",ret);
    ret = request_irq(IRQ_EINT5, s3c_slide_isr, IRQF_DISABLED, "slide pgio", (void *) pdev);

    if (ret) {
        printk("request_irq failed (IRQ_SLIDE (gpio)) !!!\n");
        ret = -EIO;
    }

    /* initial state */
    slide_is_closed = readl(S5PV210_GPH0DAT) & (1 << 5);
    /* 0 : open, 1 : closed */
    input_report_switch(pkeypad->dev, SW_LID, slide_is_closed);
    input_sync(pkeypad->dev);

    printk(KERN_INFO "\n@@ Slidegpio_isr_setup\n");
    return ret;
}

static ssize_t keyshort_test(struct device *dev, struct device_attribute *attr, char *buf)
{
    int count;
    int mask=0;
    u32 col=0,cval=0,rval=0;

    if(!gpio_get_value(S5PV210_GPH2(6)))//Power Key
    {
        mask |= 0x1;
    }

   rval = ~readl(key_base+S3C_KEYIFROW) & 0x007F ;     /* SENSE[0-6] */
    writel(KEYIFCOL_CLEAR, key_base+S3C_KEYIFCOL) ;
    if (rval)
    {
        mask |= 0x100 ;
    }
    if(/*!gpio_get_value(GPIO_KBR0) || !gpio_get_value(GPIO_KBR1) || !gpio_get_value(GPIO_KBR2) || */ mask)
    {
        count = sprintf(buf,"PRESS\n");
        printk("keyshort_test: PRESS\n",mask);
    }
    else
    {
        count = sprintf(buf,"RELEASE\n");
        printk("keyshort_test: RELEASE \n");
    }

    return count;
}

// [ KEY_LED_control_20101204
static ssize_t key_led_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
   unsigned char data;
   extern bool lightsensor_test;

   printk(KERN_ERR "key_led_control --> in\n");

   if(sscanf(buf, "%d\n", &data) == 1) {
      
//      extern int lightsensor_KeyLedLevel();  // active keyled
      printk(KERN_ERR "key_led_control: %d \n", data);

//HW Request key LED control by lightsensor and light level
// active keyled  {{
        if(data == 2)
        {
            keyLedSigOn = 0;
        }

        if(data == 1 )//&& lightsensor_KeyLedLevel() || lightsensor_test)
        {
         keypad_led_onoff(1);
        }
        else if(data == 2 )// || lightsensor_KeyLedLevel()==0)
        {
         keypad_led_onoff(0);
        }

        if(data == 1)
        {
            keyLedSigOn = 1;
        }
// active keyled  }}
  }
   else
   {
      printk(KERN_ERR "key_led_control Error\n");
   }

   return size;
}


static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, NULL, key_led_control);
// KEY_LED_control_20101204 ]


static DEVICE_ATTR(key_pressed, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, keyshort_test, NULL);

static int __init s3c_keypad_probe(struct platform_device *pdev)
{
	struct resource *res, *keypad_mem, *keypad_irq;
	struct input_dev *input_dev;
	struct s3c_keypad *s3c_keypad;
	int ret, size;
	int key, code;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev,"no memory resource specified\n");
		return -ENOENT;
	}

	size = (res->end - res->start) + 1;

	keypad_mem = request_mem_region(res->start, size, pdev->name);
	if (keypad_mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_req;
	}

	key_base = ioremap(res->start, size);
	if (key_base == NULL) {
		printk(KERN_ERR "Failed to remap register block\n");
		ret = -ENOMEM;
		goto err_map;
	}

	keypad_clock = clk_get(&pdev->dev, "keypad");
	if (IS_ERR(keypad_clock)) {
		dev_err(&pdev->dev, "failed to find keypad clock source\n");
		ret = PTR_ERR(keypad_clock);
		goto err_clk;
	}

	clk_enable(keypad_clock);

	s3c_keypad = kzalloc(sizeof(struct s3c_keypad), GFP_KERNEL);
	input_dev = input_allocate_device();

	if (!s3c_keypad || !input_dev) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	platform_set_drvdata(pdev, s3c_keypad);
	s3c_keypad->dev = input_dev;

	writel(KEYIFCON_INIT, key_base+S3C_KEYIFCON);
	writel(KEYIFFC_DIV, key_base+S3C_KEYIFFC);

	/* Set GPIO Port for keypad mode and pull-up disable*/
	s3c_setup_keypad_cfg_gpio(KEYPAD_ROWS, KEYPAD_COLUMNS);

	writel(KEYIFCOL_CLEAR, key_base+S3C_KEYIFCOL);
	// TODO:FORTE REV01_GPIO_CONTROL
   	/* GPIO_CONTROL */
    	s3c_gpio_setpin(GPIO_KEYSCAN6, GPIO_LEVEL_LOW) ;
    	s3c_gpio_setpin(GPIO_KEYSCAN7, GPIO_LEVEL_LOW) ;

	/* create and register the input driver */
	set_bit(EV_KEY, input_dev->evbit);
	/*Commenting the generation of repeat events*/
	//set_bit(EV_REP, input_dev->evbit);
	s3c_keypad->nr_rows = KEYPAD_ROWS;
	s3c_keypad->no_cols = KEYPAD_COLUMNS;
	s3c_keypad->total_keys = MAX_KEYPAD_NR;

	for(key = 0; key < s3c_keypad->total_keys; key++){
		code = s3c_keypad->keycodes[key] = keypad_keycode[key];
		if(code<=0)
			continue;
		set_bit(code & KEY_MAX, input_dev->keybit);
	}

    // remove // TODO:RE-CHECK!!!    set_bit(26 & KEY_MAX, input_dev->keybit);

   input_set_capability(input_dev, EV_SW, SW_LID); // TODO:FORTE

	input_dev->name = DEVICE_NAME;
	input_dev->phys = "s3c-keypad/input0";

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor  = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0001;

	input_dev->keycode = keypad_keycode;

	ret = input_register_device(input_dev);
	if (ret) {
		printk("Unable to register s3c-keypad input device!!!\n");
		goto out;
	}

	/* Scan timer init */
	init_timer(&keypad_timer);
	keypad_timer.function = keypad_timer_handler;
	keypad_timer.data = (unsigned long)s3c_keypad;

	/* For IRQ_KEYPAD */
	keypad_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (keypad_irq == NULL) {
		dev_err(&pdev->dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err_irq;
	}


    if(HWREV < 0x2) //yhkim block it for Verizon ATLAS.
	{
		ret = request_irq(keypad_irq->start, s3c_keypad_isr, IRQF_SAMPLE_RANDOM,
			DEVICE_NAME, (void *) pdev);
		if (ret) {
			printk("request_irq failed (IRQ_KEYPAD) !!!\n");
			ret = -EIO;
			goto err_irq;
		}

		keypad_timer.expires = jiffies + (3*HZ/100);    // 30ms

		if (is_timer_on == FALSE) {
			add_timer(&keypad_timer);
			is_timer_on = TRUE;
		} else {
			mod_timer(&keypad_timer,keypad_timer.expires);
		}
	}

    s3c_slidegpio_isr_setup((void *)s3c_keypad);
	s3c_keygpio_isr_setup((void *)s3c_keypad);

    // remove // input_report_switch(s3c_keypad->dev, SW_LID, 1);  // TODO:FORTE
    // remove // input_sync(s3c_keypad->dev); // TODO:FORTE

	printk( DEVICE_NAME " Initialized\n");

	if (device_create_file(&pdev->dev, &dev_attr_key_pressed) < 0)
	{
		printk(KERN_ERR "%s s3c_keypad_probe\n",__FUNCTION__);
		pr_err(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_key_pressed.attr.name);
	}

// [ KEY_LED_control_20101204
   	if (device_create_file(&pdev->dev, &dev_attr_brightness) < 0)
   	{
      		printk(KERN_ERR "%s s3c_keypad_probe\n",__FUNCTION__);
      		pr_err(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_brightness.attr.name);
   	}
// KEY_LED_control_20101204 ]

	return 0;

out:
	free_irq(keypad_irq->start, input_dev);
	free_irq(keypad_irq->end, input_dev);

err_irq:
	input_free_device(input_dev);
	kfree(s3c_keypad);

err_alloc:
	clk_disable(keypad_clock);
	clk_put(keypad_clock);

err_clk:
	iounmap(key_base);

err_map:
	release_resource(keypad_mem);
	kfree(keypad_mem);

err_req:
	return ret;
}

static int s3c_keypad_remove(struct platform_device *pdev)
{
	struct input_dev *input_dev = platform_get_drvdata(pdev);
	writel(KEYIFCON_CLEAR, key_base+S3C_KEYIFCON);

	if(keypad_clock) {
		clk_disable(keypad_clock);
		clk_put(keypad_clock);
		keypad_clock = NULL;
	}

	input_unregister_device(input_dev);
	iounmap(key_base);
	kfree(pdev->dev.platform_data);
	free_irq(IRQ_KEYPAD, (void *) pdev);

	del_timer(&keypad_timer);
	printk(DEVICE_NAME " Removed.\n");
	return 0;
}

#ifdef CONFIG_PM
#include <plat/pm.h>

static struct sleep_save s3c_keypad_save[] = {
	SAVE_ITEM(KEYPAD_ROW_GPIOCON),
	SAVE_ITEM(KEYPAD_COL_GPIOCON),
	SAVE_ITEM(KEYPAD_ROW_GPIOPUD),
	SAVE_ITEM(KEYPAD_COL_GPIOPUD),
};

static unsigned int keyifcon, keyiffc;

static void s3c_keypad_port_sleep(int rows, int columns)
{
	unsigned int gpio;
	unsigned int end;

    /* KEYPAD SENSE/ROW */
	end = S5PV210_GPH3(rows);
	for (gpio = S5PV210_GPH3(0) ; gpio < end ; gpio++)
    {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}

    /* KEYPAD SCAN/COLUMN */
    columns -= 2 ;  /* except scan[6~7] */
	end = S5PV210_GPH2(columns);

	for (gpio = S5PV210_GPH2(0) ; gpio < end ; gpio++)
    {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
#ifdef CONFIG_S5PV210_FORTE
		s3c_gpio_setpin(gpio, GPIO_LEVEL_LOW);
#endif /* MBjgnoh 10.12.30 Request by HW. */
	}

    s3c_gpio_cfgpin(GPIO_KEYSCAN6, S3C_GPIO_INPUT) ;
	s3c_gpio_setpull(GPIO_KEYSCAN6, S3C_GPIO_PULL_UP);
    s3c_gpio_cfgpin(GPIO_KEYSCAN7, S3C_GPIO_INPUT) ;
	s3c_gpio_setpull(GPIO_KEYSCAN7, S3C_GPIO_PULL_UP);
#ifdef CONFIG_S5PV210_FORTE
	s3c_gpio_setpin(GPIO_KEYSCAN6, GPIO_LEVEL_LOW);
	s3c_gpio_setpin(GPIO_KEYSCAN7, GPIO_LEVEL_LOW);
#endif /* MBjgnoh 10.12.30 Request by HW. */
}


static int s3c_keypad_suspend(struct platform_device *dev, pm_message_t state)
{
	keyifcon = readl(key_base+S3C_KEYIFCON);
	keyiffc = readl(key_base+S3C_KEYIFFC);

	s3c_pm_do_save(s3c_keypad_save, ARRAY_SIZE(s3c_keypad_save));

	//writel(~(0xfffffff), KEYPAD_ROW_GPIOCON);
	//writel(~(0xfffffff), KEYPAD_COL_GPIOCON);

	disable_irq(IRQ_KEYPAD);

	clk_disable(keypad_clock);

    s3c_keypad_port_sleep(KEYPAD_ROWS, KEYPAD_COLUMNS) ;

	in_sleep = 1;

	return 0;
}


static int s3c_keypad_resume(struct platform_device *dev)
{
	struct s3c_keypad          *s3c_keypad = (struct s3c_keypad *) platform_get_drvdata(dev);
    struct input_dev           *iDev = s3c_keypad->dev;
	unsigned int key_temp_data=0;

	printk(KERN_DEBUG "++++ %s\n", __FUNCTION__ );
	s3c_setup_keypad_cfg_gpio(KEYPAD_ROWS, KEYPAD_COLUMNS);

	clk_enable(keypad_clock);

	writel(KEYIFCON_INIT, key_base+S3C_KEYIFCON);
	writel(keyiffc, key_base+S3C_KEYIFFC);
	writel(KEYIFCOL_CLEAR, key_base+S3C_KEYIFCOL);

    	/* GPIO_CONTROL */
    	s3c_gpio_setpin(GPIO_KEYSCAN6, GPIO_LEVEL_HIGH) ;
    	s3c_gpio_setpin(GPIO_KEYSCAN7, GPIO_LEVEL_HIGH) ;

    	s3c_gpio_setpin(GPIO_KEYSCAN6, GPIO_LEVEL_LOW) ;
    	s3c_gpio_setpin(GPIO_KEYSCAN7, GPIO_LEVEL_LOW) ;

	s3c_pm_do_restore(s3c_keypad_save, ARRAY_SIZE(s3c_keypad_save));

	enable_irq(IRQ_KEYPAD);
	printk(KERN_DEBUG "---- %s\n", __FUNCTION__ );
	return 0;
}
#else
#define s3c_keypad_suspend NULL
#define s3c_keypad_resume  NULL
#endif /* CONFIG_PM */

static struct platform_driver s3c_keypad_driver = {
	.probe		= s3c_keypad_probe,
	.remove		= s3c_keypad_remove,
	.suspend	= s3c_keypad_suspend,
	.resume		= s3c_keypad_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-keypad",
	},
};

static int __init s3c_keypad_init(void)
{
	int ret;

	ret = platform_driver_register(&s3c_keypad_driver);

	if(!ret)
	   printk(KERN_INFO "S3C Keypad Driver\n");

	return ret;
}

static void __exit s3c_keypad_exit(void)
{
	platform_driver_unregister(&s3c_keypad_driver);
}

module_init(s3c_keypad_init);
module_exit(s3c_keypad_exit);

MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KeyPad interface for Samsung S3C");
