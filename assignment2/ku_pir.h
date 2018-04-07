#include <linux/list.h>

#define KUPIR_MAX_MSG	1000
#define KUPIR_SENSOR	17
#define DEV_NAME	"ku_pir_dev"

struct ku_pir_data {
	long unsigned int timestamp;
	int rf_flag;			//1: rising, 0: falling
};

struct my_list {
	struct list_head list;
	struct ku_pir_data data;
};

int msg_len;
