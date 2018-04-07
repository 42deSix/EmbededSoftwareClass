#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#define IOCTL_START_NUM	0x80
#define IOCTL_NONBLOCKING_NUM	IOCTL_START_NUM+1
#define IOCTL_BLOCKING_NUM	IOCTL_START_NUM+2

#define IOCTL_NUM	'z'
#define IOCTL_NONBLOCKING	_IOWR(IOCTL_NUM, IOCTL_NONBLOCKING_NUM, unsigned long *)
#define IOCTL_BLOCKING	_IOWR(IOCTL_NUM, IOCTL_BLOCKING_NUM, unsigned long *)

struct ku_pir_data {
	long unsigned int timestamp;
	int rf_flag;			//1: rising, 0: falling
};

int ku_pir_open() {
	int dev;

	dev = open("/dev/ku_pir_dev", O_RDWR);

	if(dev<=0)
		return 0;
	return dev;

	//success : file descripter (positive integer)
	//fail : 0	
}

int ku_pir_close(int fd) {
	int ret;

	if(fd <= 0)
		return 0;

	ret = close(fd);

	if(ret == -1)
		return 0;

	return fd;

	//success : positive integer (i set this to fd)
	//fail : 0
}

struct ku_pir_data * ku_pir_nonblocking(int fd, long unsigned int ts) {
	int ret;
	struct ku_pir_data *data;

	data = (struct ku_pir_data *)malloc(sizeof(struct ku_pir_data));
	data->timestamp = ts;

	ret = ioctl(fd, IOCTL_NONBLOCKING, (unsigned long *)data);

	if(ret == -1) {
		free(data);
		return NULL;
	}
	return data;

	//fail : return NULL
	//success : return data
}

struct ku_pir_data * ku_pir_blocking(int fd, long unsigned int ts) {
	int ret;
	struct ku_pir_data *data;

	data = (struct ku_pir_data *)malloc(sizeof(struct ku_pir_data));
	data->timestamp = ts;

	ret = ioctl(fd, IOCTL_BLOCKING, (unsigned long *)data);

	if(ret == -1) {
		free(data);
		return NULL;
	}
	return data;

	//fail : return NULL
	//success : return data
	//it will be not fail
}

int ku_pir_insertData(int fd, long unsigned int ts, int rf_flag) {

	int ret;

	struct ku_pir_data myData;

	myData.timestamp = ts;
	myData.rf_flag = rf_flag;

	ret = write(fd, (char *)&myData, sizeof(struct ku_pir_data));

	return ret;		//success 1, fail 0
}
