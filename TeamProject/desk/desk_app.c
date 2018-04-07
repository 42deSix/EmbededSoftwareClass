#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"

#define DEV_NAME "desk_dev"

#define ADDRESS     "192.168.0.7"
#define CLIENTID    "pi0"
//#define CLIENTID	"pi1"

#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1
#define IOCTL_NUM2 IOCTL_START_NUM+2

#define SIMPLE_IOCTL_NUM 'z'
#define BREAK _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM1, unsigned long *)
#define CALL _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM2, unsigned long *)

int dev;

void publish(MQTTClient client, char* topic, char* payload) {
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = payload;
        pubmsg.payloadlen = strlen(pubmsg.payload);
        pubmsg.qos = 2;
        pubmsg.retained = 0;
        MQTTClient_deliveryToken token;
        MQTTClient_publishMessage(client, topic, &pubmsg, &token);
        MQTTClient_waitForCompletion(client, token, 1000L);
        printf("Message '%s' with delivery token %d delivered\n", payload, token);
}

int on_message(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
        char* payload = message->payload;
        printf("Received operation %s\n", payload);

	// break of call - ioctl
        if(strcmp(payload, "break") == 0) {
                ioctl(dev, BREAK, 0);
        } else if(strcmp(payload, "call") == 0) {
                ioctl(dev, CALL, 0);
        }

        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
}

void main(void) {
        MQTTClient client;
        MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
        MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
        conn_opts.username = "1/2";
        conn_opts.password = "3";

        MQTTClient_setCallbacks(client, NULL, NULL, on_message, NULL);

        char *pir_svalue;
        int pir_value = 0;
        int position = 0;
	//int position = 1;
        int rc;
        if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
                printf("Failed to connect, return code %d\n", rc);
                exit(-1);
        }

        dev = open("/dev/desk_dev", O_RDWR);

        MQTTClient_subscribe(client, "speaker", 0);
        MQTTClient_subscribe(client, "speaker0", 0);
        //MQTTClient_subscribe(client, "speaker1", 0);
        printf("masked value : %d\n", position & pir_value);
        for(;;) {
		//get the pir_value
                pir_value = read(dev, NULL, 0);
		if(pir_value != -1) {
			if(pir_value == 0) {
				pir_svalue = "off";
			} else {
				pir_svalue = "on";
			}

			// send to office the value of pir
			if(position == 0) {
				publish(client, "pir0", pir_svalue);
			} else {
				publish(client, "pir1", pir_svalue);
			}
		}
                sleep(1);
        }

        MQTTClient_disconnect(client, 1000);
        MQTTClient_destroy(&client);

        close(dev);

}


