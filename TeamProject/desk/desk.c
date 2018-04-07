#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/sched.h>
#include <linux/kthread.h>

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/cdev.h>
//#include <asm/delay.h>

MODULE_LICENSE("GPL");

#define DEV_NAME "desk_dev"


#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1
#define IOCTL_NUM2 IOCTL_START_NUM+2

#define SIMPLE_IOCTL_NUM 'z'
#define BREAK _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM1, unsigned long *)
#define CALL _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM2, unsigned long *)


#define LED	4
#define PIR	17
#define SPEAKER	22

static int pir_value = -1;
static int irq_num;

int baseTab[] = {1911,1702,1516,1431,1275,1136,1012};

static long desk_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	int i,j;
	switch(cmd) {
	case BREAK:				//break time start or end : speaker play
		for(j=0; j<2; j++) {
			for(i=0; i<70; i++) {
				gpio_set_value(SPEAKER, 1);
				udelay(baseTab[0]);
				gpio_set_value(SPEAKER, 0);
				udelay(baseTab[0]);
			}
			for(i=0; i<70; i++) {
				gpio_set_value(SPEAKER, 1);
				udelay(baseTab[3]);
				gpio_set_value(SPEAKER, 0);
				udelay(baseTab[3]);
			}
			for(i=0; i<70; i++) {
				gpio_set_value(SPEAKER, 1);
				udelay(baseTab[5]);
				gpio_set_value(SPEAKER, 0);
				udelay(baseTab[5]);
			}
			for(i=0; i<70; i++) {
				gpio_set_value(SPEAKER, 1);
				udelay(baseTab[4]);
				gpio_set_value(SPEAKER, 0);
				udelay(baseTab[4]);
			}
		}
		break;
	case CALL:					//call : speaker ring
		for(j=0; j<4; j++) {
			for(i=0; i<70; i++) {
				gpio_set_value(SPEAKER, 1);
				udelay(baseTab[6]);
				gpio_set_value(SPEAKER, 0);
				udelay(baseTab[6]);
			}
		}
		break;
	default:
		return -1;
	}

	return 0;
}

static int desk_open(struct inode *inode, struct file *file) {
        printk("desk open\n");
        return 0;
}

static int desk_release(struct inode *inode, struct file *file) {
        printk("desk release\n");
        return 0;
}

/*
 * just return pir_value
 * for mqtt send to office in the user app
 */
static int pir_read(struct file *file, char *buf, size_t len, loff_t *lof) {
	return pir_value;
}

struct file_operations simple_char_fops = {
	.unlocked_ioctl = desk_ioctl,
	.read = pir_read,
        .open = desk_open,
        .release = desk_release,
};

//rising or falling, get the pir value. just save to pir_value
//and set the LED
static irqreturn_t pir_isr(int irq, void* dev_id) {
	int i;
	pir_value = gpio_get_value(PIR);

	gpio_set_value(LED, pir_value);

	return IRQ_HANDLED;
}

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init desk_init(void) {
	int ret;
	int i;

	printk("Init Module\n");
	
	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &simple_char_fops);
	cdev_add(cd_cdev, dev_num, 1);

	gpio_request_one(PIR, GPIOF_IN, "pir");
	irq_num = gpio_to_irq(PIR);
	ret = request_irq(irq_num, pir_isr, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "pir_irq", NULL);
	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n",ret);
		free_irq(irq_num, NULL);
	}

	gpio_request_one(LED, GPIOF_OUT_INIT_LOW, "led");
	gpio_request_one(SPEAKER, GPIOF_OUT_INIT_LOW, "speaker");

        return 0;
}

static void __exit desk_exit(void) {
        printk("Exit Module\n");

	free_irq(irq_num,NULL);
	gpio_free(PIR);
	gpio_free(LED);
	gpio_free(SPEAKER);

	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num,1);
}

module_init(desk_init);
module_exit(desk_exit);
