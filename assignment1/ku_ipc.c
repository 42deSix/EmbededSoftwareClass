#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>

#include <linux/uaccess.h>

MODULE_LICENSE("GPL");

#include "ku_ipc.h"

#define IOCTL_START_NUM	0x80
#define IOCTL_MSGGET_CREAT_NUM	IOCTL_START_NUM+1
#define IOCTL_MSGGET_EXCL_NUM	IOCTL_START_NUM+2
#define IOCTL_MSGCLOSE_NUM	IOCTL_START_NUM+3

#define IOCTL_NUM	'z'
#define IOCTL_MSGGET_CREAT	_IOWR(IOCTL_NUM, IOCTL_MSGGET_CREAT_NUM, unsigned long *)
#define IOCTL_MSGGET_EXCL	_IOWR(IOCTL_NUM, IOCTL_MSGGET_EXCL_NUM, unsigned long *)
#define IOCTL_MSGCLOSE	_IOWR(IOCTL_NUM, IOCTL_MSGCLOSE_NUM, unsigned long *)

#define DEV_NAME "ku_ipc_dev"

struct msg_list mylist;

spinlock_t my_lock;

static int ku_ipc_open(struct inode *inode, struct file *file) {
	printk("ku_ipc_opened\n");
	return 0;
}

static int ku_ipc_release(struct inode *inode, struct file *file) {
	printk("ku_ipc_released\n");
	return 0;
}

static int ku_ipc_read(struct file *file, char *buf, size_t len, loff_t *lof) {
	struct msg_list * tmp = 0;
	struct list_head * pos = 0;
	struct list_head * q = 0;

	void *msg;
	int *msqid;
	int *msgflg;
	long *msgtyp;
	int ret = 0;
	int i,j;

	msg = (void *)kmalloc(len+sizeof(long), GFP_KERNEL);
	msqid = (int *)kmalloc(sizeof(int), GFP_KERNEL);
	msgflg = (int *)kmalloc(sizeof(int), GFP_KERNEL);
	msgtyp = (long *)kmalloc(sizeof(long), GFP_KERNEL);
	copy_from_user(msg, ((struct data *)buf)->msgp, len+sizeof(long));
	copy_from_user(msqid, &(((struct data *)buf)->msqid), sizeof(int));
	copy_from_user(msgflg, &(((struct data *)buf)->msgflg), sizeof(int));
	copy_from_user(msgtyp, &(((struct data *)buf)->msgtyp), sizeof(long));
	
	if(len > KUIPC_MAXVOL) {
		printk("Error - MAXVOL\n");
		kfree(msgtyp);
		kfree(msg);
		kfree(msqid);
		kfree(msgflg);
		return -1;
	} else if(len <= 0) {
		printk("Error - size must be bigger than 0\n");
		kfree(msgtyp);
		kfree(msg);
		kfree(msqid);
		kfree(msgflg);
		return -1;
	}

	list_for_each(pos, &(mylist.list))
	{
		tmp = list_entry(pos, struct msg_list, list);
		if(tmp->msqid == *msqid) {
			spin_lock(&tmp->lock);
			if(tmp->len <= 0) {
				printk("no data in queue\n");
				kfree(msgtyp);
				kfree(msg);
				kfree(msqid);
				kfree(msgflg);
				spin_unlock(&tmp->lock);
				return -1;
			} else {
				for(i=0; i<tmp->len; i++) {
					if(tmp->type[i] == *msgtyp) {
						if(tmp->size[i] > len && ((*msgflg) == MSG_NOERROR)) {
							printk("Error - bigger size msg\n");
							kfree(msgtyp);
							kfree(msg);
							kfree(msqid);
							kfree(msgflg);
							spin_unlock(&tmp->lock);
							return -1;
						}
						copy_to_user(((struct data *)buf)->msgp, &(tmp->type[i]), sizeof(long));
						copy_to_user((((struct data *)buf)->msgp)+4, tmp->msg[i], tmp->size[i]);
						ret = tmp->size[i];
						tmp->len--;

						for(j=i; j<tmp->len; j++) {
							tmp->msg[j] = tmp->msg[j+1];
							tmp->size[j] = tmp->size[j+1];
							tmp->type[j] = tmp->type[j+1];
						}

						kfree(msgtyp);
						kfree(msg);
						kfree(msqid);
						kfree(msgflg);
						spin_unlock(&tmp->lock);
						return ret;
					}
				}

				printk("Error - no type %ld\n",*msgtyp);
				kfree(msgtyp);
				kfree(msg);
				kfree(msqid);
				kfree(msgflg);
				spin_unlock(&tmp->lock);
				return -1;
			}
		}
	}

	printk("Error - queue not found\n");
	kfree(msgtyp);
	kfree(msg);
	kfree(msqid);
	kfree(msgflg);
	return -1;
}

static int ku_ipc_write(struct file *file, char *buf, size_t len, loff_t *lof) {
	struct msg_list * tmp = 0;
	struct list_head * pos = 0;

	int i;

	void *msg;
	int *msqid;
	int *msgflg;

	msg = (void *)kmalloc(len, GFP_KERNEL);
	msqid = (int *)kmalloc(sizeof(int), GFP_KERNEL);
	msgflg = (int *)kmalloc(sizeof(int), GFP_KERNEL);
	copy_from_user(msg, (((struct data *)buf)->msgp), len);
	copy_from_user(msqid, &(((struct data *)buf)->msqid), sizeof(int));
	copy_from_user(msgflg, &(((struct data *)buf)->msgflg), sizeof(int));

	if(len > KUIPC_MAXVOL) {
		printk("Error - MAXVOL\n");
		kfree(msqid);
		kfree(msgflg);
		return -1;
	} else if(len <= 0) {
		printk("Error - size must be bigger than 0\n");
		kfree(msqid);
		kfree(msgflg);
		return -1;
	}

	list_for_each(pos, &(mylist.list))
	{
		tmp = list_entry(pos, struct msg_list, list);
		if(tmp->msqid == *msqid) {
			printk("found msq - %d\n",tmp->msqid);
			spin_lock(&tmp->lock);
			if(tmp->len >= KUIPC_MAXMSG) {
				if(*msgflg == IPC_NOWAIT) {
					printk("Queue has no space more\n");
					kfree(msqid);
					kfree(msgflg);
					spin_unlock(&tmp->lock);
					return -1;
				} else {
					spin_unlock(&tmp->lock);
					while(tmp->len >= KUIPC_MAXMSG);
					spin_lock(&tmp->lock);
				}
			}
			tmp->type[tmp->len] = *((long*)msg);
			tmp->msg[tmp->len] = msg+4;
			tmp->size[tmp->len] = len;
			tmp->len++;
			spin_unlock(&tmp->lock);

			printk("MSG added successfully!!!\n");
			
			kfree(msqid);
			kfree(msgflg);
			return 0;
		}
	}

	printk("Error - queue not found\n");
	kfree(msqid);
	kfree(msgflg);
	return -1;
}

static long ku_ipc_ioctl(struct file *file, unsigned int cmd, unsigned long *arg) {
	struct msg_list * tmp = 0;
	struct list_head * pos = 0;
	int i;

	switch(cmd) {
	case IOCTL_MSGGET_CREAT:
		printk("IOCTL_MSGGET_CREAT\n");
		list_for_each(pos, &(mylist.list))
		{
			tmp = list_entry(pos, struct msg_list, list);
			if(tmp->msqid == *arg+100) {
				return tmp->msqid;
			}
		}
		tmp = (struct msg_list *)kmalloc(sizeof(struct msg_list), GFP_KERNEL);
		tmp->msqid = *arg+100;
		for(i=0; i<KUIPC_MAXMSG; i++) {
			tmp->type[i] = 0;
			tmp->size[i] = 0;
			tmp->msg[i] = (void *)kmalloc(KUIPC_MAXMSG*KUIPC_MAXVOL, GFP_KERNEL);
			memset(tmp->msg[i], 0, KUIPC_MAXMSG);
		}
		tmp->len = 0;

		spin_lock_init(&tmp->lock);
		list_add(&(tmp->list),&(mylist.list));

		return *arg+100;

	case IOCTL_MSGGET_EXCL:
		printk("IOCTL_MSGGET_EXCL\n");
		list_for_each(pos, &(mylist.list))
		{
			tmp = list_entry(pos, struct msg_list, list);
			if(tmp->msqid == *arg+100) {
				return -1;
			}
		}
		tmp = (struct msg_list *)kmalloc(sizeof(struct msg_list), GFP_KERNEL);
		tmp->msqid = *arg+100;
		for(i=0; i<KUIPC_MAXMSG; i++) {
			tmp->type[i] = 0;
			tmp->size[i] = 0;
			tmp->msg[i] = (void *)kmalloc(KUIPC_MAXMSG*KUIPC_MAXVOL, GFP_KERNEL);
			memset(tmp->msg, 0, KUIPC_MAXMSG);
		}
		tmp->len = 0;
		tmp->len = 0;

		spin_lock_init(&tmp->lock);
		list_add(&(tmp->list),&(mylist.list));

		return *arg+100;

	case IOCTL_MSGCLOSE:
		printk("IOCTL_MSGCLOSE\n");
		list_for_each(pos, &(mylist.list))
		{
			tmp = list_entry(pos, struct msg_list, list);
			if(tmp->msqid == *arg) {
				return 0;
			}
		}

		return -1;

	default:
		printk("IOCTL_DEFAULT\n");
		return -1;
	}
	
	return 0;
}

struct file_operations ku_ipc_fops = {
	.unlocked_ioctl = ku_ipc_ioctl,
	.open = ku_ipc_open,
	.release = ku_ipc_release,
	.read = ku_ipc_read,
	.write = ku_ipc_write,
};

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init ku_ipc_kernel_init(void){
	int ret;

	printk("Init Module\n");

	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &ku_ipc_fops);
	ret = cdev_add(cd_cdev, dev_num, 1);
	if( ret < 0 ){
		printk("fail to add character device\n");
		return -1;
	}

	INIT_LIST_HEAD(&(mylist.list));
	
	spin_lock_init(&my_lock);

	return 0;
}

static void __exit ku_ipc_kernel_exit(void){
	struct list_head *pos = 0;
	struct list_head *q = 0;
	struct msg_list *tmp = 0;
	int i;
	printk("Exit Module\n");

	list_for_each_safe(pos, q, &(mylist.list))
	{
		tmp = list_entry(pos, struct msg_list, list);
		list_del(pos);
		for(i=0; i<KUIPC_MAXMSG; i++) {
			kfree(tmp->msg[i]);
		}
		kfree(tmp);
	}

	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num, 1);	
}



module_init(ku_ipc_kernel_init);
module_exit(ku_ipc_kernel_exit);
