#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"

#define DEV_NAME "office_dev"

#define ADDRESS     "192.168.0.7"
#define CLIENTID    "office"


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

int dev;
int led0=0, led1=0;

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
	int input;
	unsigned long value;
	char* payload = message->payload;
	printf("Received operation %s\n", payload);

	//decide led on, off
	if(strcmp(payload, "off") == 0) {
		value = 0;
	} else {
		value = 1;
	}

	// use ioctl to set value of led
	if(strcmp(topicName, "pir0") == 0) {
		ioctl(dev, LED0_IOCTL, &value);
	} else {
		ioctl(dev, LED1_IOCTL, &value);
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
        int btn0_value = 0;
	int btn1_value = 0;
        int position = 0;
        int rc;
        if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
                printf("Failed to connect, return code %d\n", rc);
                exit(-1);
        }

        dev = open("/dev/office_dev", O_RDWR);

        MQTTClient_subscribe(client, "pir0", 0);
        MQTTClient_subscribe(client, "pir1", 0);
        printf("masked value : %d\n", position & btn0_value);
        for(;;) {
		// value 1 - if btn was pressed
                btn0_value = ioctl(dev, BTN0_IOCTL, 0);
		btn1_value = ioctl(dev, BTN1_IOCTL, 0);
		if(btn0_value == 1) {
			pir_svalue = "call";
			publish(client, "speaker0", pir_svalue);
		}
		if(btn1_value == 1) {
			pir_svalue = "call";
			publish(client, "speaker1", pir_svalue);
		}

		// if break time start or end, value is 1
		if(ioctl(dev, BREAK_TIME, 0)) {
	                pir_svalue = "break";
			publish(client, "speaker", pir_svalue);
		}

                sleep(1);
        }
        //listen for operation

        MQTTClient_disconnect(client, 1000);
        MQTTClient_destroy(&client);

        close(dev);

}


