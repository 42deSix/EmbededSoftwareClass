#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/sched.h>
#include <linux/kthread.h>

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include <linux/uaccess.h>

#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/cdev.h>

#include <linux/timer.h>

MODULE_LICENSE("GPL");

#define DEV_NAME "office_dev"


#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1
#define IOCTL_NUM2 IOCTL_START_NUM+2
#define IOCTL_NUM3 IOCTL_START_NUM+3
#define IOCTL_NUM4 IOCTL_START_NUM+4
#define IOCTL_NUM5 IOCTL_START_NUM+5

#define SIMPLE_IOCTL_NUM 'z'
#define BTN0_IOCTL _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM1, unsigned long *)
#define BTN1_IOCTL _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM2, unsigned long *)
#define BREAK_TIME _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM3, unsigned long *)
#define LED0_IOCTL _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM4, unsigned long *)
#define LED1_IOCTL _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM5, unsigned long *)


#define LED0	18
#define LED1	24
#define BTN0	17
#define BTN1	27

static int btn0_value = -1;
static int btn1_value = -1;
static int irq_num0;
static int irq_num1;

static struct timer_list break_timer;

static int isBreakTime = 0;
static int shouldRinging = 0;

/*
 * when break time start or end,
 * set the value "shouldRinging"
 * to play the speaker of desks
 */
static void break_timer_func(unsigned long data) {
	if(isBreakTime) {
		break_timer.expires = jiffies + (20*HZ);
		add_timer(&break_timer);
	} else {
		break_timer.expires = jiffies + (10*HZ);
		add_timer(&break_timer);
	}

	shouldRinging = 1;

	isBreakTime = !isBreakTime;
}

static int office_open(struct inode *inode, struct file *file) {
        printk("office open\n");
        return 0;
}

static int office_release(struct inode *inode, struct file *file) {
        printk("office release\n");
        return 0;
}

static long office_ioctl(struct file *file, unsigned int cmd, unsigned long *arg) {
	printk("ioclt\n");
	switch(cmd) {
	// for desk 0 call
	case BTN0_IOCTL:
		if(btn0_value == 1) {
			btn0_value = 0;
			return 1;
		}
		return 0;
		break;
	// for desk 1 call
	case BTN1_IOCTL:
		if(btn1_value == 1) {
			btn1_value = 0;
			return 1;
		}
		return 0;
		break;
	case BREAK_TIME:
		if(shouldRinging == 1) {
			shouldRinging = 0;
			return 1;
		}
		break;
	case LED0_IOCTL:
		gpio_set_value(LED0, *arg);
		break;
	case LED1_IOCTL:
		gpio_set_value(LED1, *arg);
		break;
	default:
		return -1;
	}

	return 0;
}

struct file_operations simple_char_fops = {
	.unlocked_ioctl = office_ioctl,
        .open = office_open,
        .release = office_release,
};

static irqreturn_t btn0_isr(int irq, void* dev_id) {
	btn0_value = 1;

	return IRQ_HANDLED;
}

static irqreturn_t btn1_isr(int irq, void* dev_id) {
	btn1_value = 1;
	
	return IRQ_HANDLED;
}

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init office_init(void) {
	int ret;

	printk("Init Module\n");
	
	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &simple_char_fops);
	cdev_add(cd_cdev, dev_num, 1);

	gpio_request_one(BTN0, GPIOF_IN, "btn0");
	irq_num0 = gpio_to_irq(BTN0);
	ret = request_irq(irq_num0, btn0_isr, IRQF_TRIGGER_FALLING, "btn0_irq", NULL);
	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n",ret);
		free_irq(irq_num0, NULL);
	}

	gpio_request_one(BTN1, GPIOF_IN, "btn1");
	irq_num1 = gpio_to_irq(BTN1);
	ret = request_irq(irq_num1, btn1_isr, IRQF_TRIGGER_FALLING, "btn1_irq", NULL);
	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n",ret);
		free_irq(irq_num1, NULL);
	}

	gpio_request_one(LED0, GPIOF_OUT_INIT_LOW, "led0");
	gpio_request_one(LED1, GPIOF_OUT_INIT_LOW, "led1");
	
	//set timer for break time
	init_timer(&break_timer);
	break_timer.function = break_timer_func;
	break_timer.expires = jiffies + (20*HZ);

	add_timer(&break_timer);

        return 0;
}

static void __exit office_exit(void) {
        printk("Exit Module\n");

	del_timer(&break_timer);

	free_irq(irq_num0,NULL);
	free_irq(irq_num1,NULL);
	gpio_free(LED0);
	gpio_free(LED1);
	gpio_free(BTN0);
	gpio_free(BTN1);

	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num,1);
}

module_init(office_init);
module_exit(office_exit);
