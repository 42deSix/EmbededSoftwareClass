#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/slab.h>

#define KUIPC_MAXMSG	10
#define KUIPC_MAXVOL	300
#define IPC_CREAT	0x80
#define IPC_EXCL	0x40
#define IPC_NOWAIT	0x20
#define MSG_NOERROR	0x10

struct msg_list {
	struct list_head list;
	int msqid;
	spinlock_t lock;
	long type[KUIPC_MAXMSG];
	void *msg[KUIPC_MAXMSG];
	size_t size[KUIPC_MAXMSG];
	int len;
};

struct data {
	void *msgp;
	long msgtyp;
	int msqid;
	int msgflg;
};
