#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>

#include <linux/slab.h>

#include <linux/uaccess.h>

#include <linux/wait.h>
#include <linux/sched.h>

#include <linux/gpio.h>
#include <linux/interrupt.h>

#include <linux/rculist.h>

MODULE_LICENSE("GPL");

#include "ku_pir.h"

#define IOCTL_START_NUM	0x80
#define IOCTL_NONBLOCKING_NUM	IOCTL_START_NUM+1
#define IOCTL_BLOCKING_NUM	IOCTL_START_NUM+2

#define IOCTL_NUM	'z'
#define IOCTL_NONBLOCKING	_IOWR(IOCTL_NUM, IOCTL_NONBLOCKING_NUM, unsigned long *)
#define IOCTL_BLOCKING	_IOWR(IOCTL_NUM, IOCTL_BLOCKING_NUM, unsigned long *)

struct my_list mylist;
spinlock_t my_lock;
DECLARE_WAIT_QUEUE_HEAD(waitqueue_t);

static int irq_num;

static int ku_pir_open(struct inode *inode, struct file *file) {
	printk("ku_pir_opened\n");
	return 0;
}

static int ku_pir_release(struct inode *inode, struct file *file) {
	printk("ku_pir_released\n");
	return 0;
}

static long ku_pir_ioctl(struct file *file, unsigned int cmd, unsigned long *arg) {
	struct my_list *pos=0;

	long unsigned int ts=0;
	int ret;

	ret = copy_from_user(&ts, &(((struct ku_pir_data *)arg)->timestamp), sizeof(long unsigned int));

	if(ret == -EFAULT)
		return -1;

	switch(cmd) {
	case IOCTL_NONBLOCKING:
		rcu_read_lock();
		list_for_each_entry_rcu(pos, &(mylist.list), list) {
			if(pos->data.timestamp > ts) {
				ret = copy_to_user(arg, &(pos->data), sizeof(struct ku_pir_data));
				if(ret == -EFAULT)
					return -1;
				return 0;
			}
		}
		rcu_read_unlock();
		break;
	case IOCTL_BLOCKING:
		while(1) {
			rcu_read_lock();
			list_for_each_entry_rcu(pos, &(mylist.list), list) {
				if(pos->data.timestamp > ts) {
					ret = copy_to_user(arg, &(pos->data), sizeof(struct ku_pir_data));
					if(ret == -EFAULT)
						return -1;
					return 0;
				}
			}
			rcu_read_unlock();
			wait_event(waitqueue_t, 1);
		}

		break;
	default:
		return -1;
	}

	return -1;
}

static irqreturn_t ku_pir_handler(int irq, void* dev_id) {
	struct my_list *new;

	struct my_list *tmp = 0;
	struct my_list *pos = 0;

	new = (struct my_list*)kmalloc(sizeof(struct my_list), GFP_ATOMIC);

	new->data.timestamp = jiffies;
	new->data.rf_flag = 10;

	if(gpio_get_value(KUPIR_SENSOR) == 1)
		new->data.rf_flag = 0;
	else if(gpio_get_value(KUPIR_SENSOR) == 0)
		new->data.rf_flag = 1;

	if(msg_len == KUPIR_MAX_MSG) {
		list_for_each_entry_safe(tmp, pos, &(mylist.list), list)
		{
			list_del_rcu(&(tmp->list));
			kfree(tmp);
			msg_len--;
			break;
		}
	}

	list_add_tail_rcu(&(new->list), &(mylist.list));
	msg_len++;
	
	wake_up(&waitqueue_t);

	return IRQ_HANDLED;
}

static int ku_pir_write(struct file *file, char *buf, size_t len, loff_t *lof) {
	struct my_list *new;

	struct my_list * tmp = 0;
	struct my_list * pos = 0;

	int ret;

	new = (struct my_list*)kmalloc(sizeof(struct my_list), GFP_KERNEL);

	ret = copy_from_user(&(new->data), (struct ku_pir_data *)buf, sizeof(struct ku_pir_data));

	if(ret == -EFAULT)
		return 0;

	if(msg_len == KUPIR_MAX_MSG) {
		list_for_each_entry_safe(tmp, pos, &(mylist.list), list)	
		{
			list_del(&(tmp->list));
			kfree(tmp);
			msg_len--;
			break;
		}
	}

	list_add_tail_rcu(&(new->list), &(mylist.list));
	msg_len++;
	
	wake_up(&waitqueue_t);

	return 1;

	//success 1, fail 0
}

struct file_operations fops = {
	.open = ku_pir_open,
	.release = ku_pir_release,
	.unlocked_ioctl = ku_pir_ioctl,
	.write = ku_pir_write,
};

static dev_t dev_num;
static struct cdev_t *cd_cdev;

static int __init ku_pir_init(void) {
	int ret;

	printk("Init module\n");

	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &fops);
	ret = cdev_add(cd_cdev, dev_num, 1);

	if(ret < 0) {
		printk("fail to add ku_pir cdev\n");
		return -1;
	}

	INIT_LIST_HEAD(&mylist.list);
	msg_len = 0;

	//add sensor gpio, etc...
	
	gpio_request_one(KUPIR_SENSOR, GPIOF_IN, "sensor1");
	irq_num = gpio_to_irq(KUPIR_SENSOR);
	ret = request_irq(irq_num, ku_pir_irq, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "sensor_PIR", NULL);
	if(ret) {
		printk(KERN_ERR "Unable to request IRQ : %d\n",ret);
		free_irq(irq_num, NULL);
		return 0;
	} else {
		//disable_irq(irq_num);
	}

	return 0;
}

static void __exit ku_pir_exit(void) {
	struct my_list *pos = 0;
	struct my_list *tmp = 0;
	int i;
	printk("Exit Module\n");

	list_for_each_entry_safe(tmp, pos, &(mylist.list), list)
	{
		list_del_rcu(&(tmp->list));
		kfree(tmp);
	}

	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num, 1);

	free_irq(irq_num, NULL);
	gpio_free(KUPIR_SENSOR);
}

module_init(ku_pir_init);
module_exit(ku_pir_exit);
