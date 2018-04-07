#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#define IOCTL_START_NUM	0x80
#define IOCTL_MSGGET_CREAT_NUM	IOCTL_START_NUM+1
#define IOCTL_MSGGET_EXCL_NUM	IOCTL_START_NUM+2
#define IOCTL_MSGCLOSE_NUM	IOCTL_START_NUM+3

#define IOCTL_NUM	'z'
#define IOCTL_MSGGET_CREAT _IOWR(IOCTL_NUM, IOCTL_MSGGET_CREAT_NUM, unsigned long *)
#define IOCTL_MSGGET_EXCL _IOWR(IOCTL_NUM, IOCTL_MSGGET_EXCL_NUM, unsigned long *)
#define IOCTL_MSGCLOSE _IOWR(IOCTL_NUM, IOCTL_MSGCLOSE_NUM, unsigned long *)

#define DEV_NAME "ku_ipc_dev"

#define IPC_CREAT	0x80
#define IPC_EXCL	0x40
#define IPC_NOWAIT	0x20
#define MSG_NOERROR	0x10

int ku_ipc_msgget(int key, int msgflg) {
	int dev;
	unsigned long value = key;
	int ret;
	
	dev = open("/dev/ku_ipc_dev", O_RDWR);
	
	if(msgflg == IPC_CREAT) {
		ret = ioctl(dev, IOCTL_MSGGET_CREAT, &value);
	} else if(msgflg == IPC_EXCL) {
		ret = ioctl(dev, IOCTL_MSGGET_EXCL, &value);
	} else {
		ret = -1;
	}

	close(dev);

	return ret;
}

int ku_msgclose(int msqid) {
	int dev;
	int ret;
	unsigned long value = msqid;

	dev = open("/dev/ku_ipc_dev", O_RDWR);

	ret = ioctl(dev, IOCTL_MSGCLOSE, &value);

	close(dev);

	return ret;
}

int ku_msgsnd(int msqid, void *msgp, int msgsz, int msgflg) {
	int dev;
	int ret;

	struct data {
		void *msgp;
		long msgtyp;
		int msqid;
		int msgflg;
	} myData;

	dev = open("/dev/ku_ipc_dev", O_RDWR);

	myData.msgp = msgp;
	myData.msqid = msqid;
	myData.msgflg = msgflg;

	ret = write(dev, (char*)&myData, msgsz);

	close(dev);

	return ret;
}

int ku_msgrcv(int msqid, void *msgp, int msgsz, long msgtyp, int msgflg) {
	int dev;
	int ret;

	struct data {
		void *msgp;
		long msgtyp;
		int msqid;
		int msgflg;
	} myData;

	dev = open("/dev/ku_ipc_dev", O_RDWR);

	myData.msgp = msgp;
	myData.msgtyp = msgtyp;
	myData.msqid = msqid;
	myData.msgflg = msgflg;

	ret = read(dev, (char*)&myData, msgsz);

	close(dev);

	return ret;
}
