#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"
#include "iotc_device_client.h"

#define HOST_URL_FORMAT "ssl://%s:8883"

#ifndef MQTT_PUBLISH_TIMEOUT_MS
#define MQTT_PUBLISH_TIMEOUT_MS     10000L
#endif

static bool is_initialized = false;
static MQTTClient client = NULL;
static char* publish_topic;
static char* ack_topic;
static char* subs_topic;
static char* di_cmd_topic; //= "devices/lnviot-windemo/messages/events/cd=QGCJED1&v=2.1&di=1";
static char* twin_dis_topic = "$iothub/twin/PATCH/properties/desired/#";
static char* twin_rpt_topic = "$iothub/twin/PATCH/properties/reported/?$rid=1";
static char* hb_pub_topic; //= "devices/lnviot-windemo/messages/events/cd=QGCJED1&v=2.1&mt=5";
static IotConnectC2dCallback c2d_msg_cb = NULL; // callback for inbound messages
static IotConnectTwinC2dCallback c2d_twin_cb = NULL;//edit twin
static IotConnectStatusCallback status_cb = NULL; // callback for connection status

static void paho_deinit() {
    if (client) {
        MQTTClient_destroy(&client);
        client = NULL;
    }
    free(publish_topic);
    publish_topic = NULL;
    free(ack_topic);
    ack_topic = NULL;
    free(di_cmd_topic);
    di_cmd_topic = NULL;
    free(hb_pub_topic);
    hb_pub_topic = NULL;
    c2d_msg_cb = NULL;
    c2d_twin_cb = NULL;// edit twin
    status_cb = NULL;
}

static int on_c2d_message(void* context, char* topicName, int topicLen, MQTTClient_message* message)
{
    //char* twin_topic = "$iothub/twin/PATCH/properties/desired/?$version=14";
    if (strncmp(topicName, twin_dis_topic, 36) == 0)
    {
        if (c2d_twin_cb)
        {
            c2d_twin_cb(message->payload, message->payloadlen);
        }
    }
    if (strncmp(topicName, subs_topic, 30) == 0)
    {
        if (c2d_msg_cb)
        {
            c2d_msg_cb(message->payload, message->payloadlen);
        }

    }

    /*
    if (c2d_msg_cb) {
        c2d_msg_cb(message->payload, message->payloadlen);
    }*/
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

/*
static int on_twin_c2d_message(void* context, char* topicName, int topicLen, MQTTClient_message* message)//edit twin
{
    if (c2d_twin_cb) {
        printf("topic name %s\n", topicName);
        c2d_twin_cb(message->payload, message->payloadlen);
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
} *///

static void on_connection_lost(void* context, char* cause) {
    printf("MQTT Connection lost. Cause: %s\n", cause);

    if (status_cb) {
        status_cb(IOTC_CS_MQTT_DISCONNECTED);
    }
    paho_deinit();
}

int iotc_device_client_disconnect() {
    int rc;
    is_initialized = false;
    if ((rc = MQTTClient_disconnect(client, 10000)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to disconnect, return code %d\n", rc);
    }
    paho_deinit();
    return rc;
}

void iotc_device_client_receive() {
    ; // do nothing for paho
}

bool iotc_device_client_is_connected() {
    if (!is_initialized) {
        return false;
    }
    return MQTTClient_isConnected(client);
}

int iotc_device_client_send_message_qos(const char* message, int qos) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    pubmsg.payload = (void*)message;
    pubmsg.payloadlen = (int)strlen(message);
    pubmsg.qos = qos;
    pubmsg.retained = 0;
    if ((rc = MQTTClient_publishMessage(client, publish_topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
        return rc;
    }

    rc = MQTTClient_waitForCompletion(client, token, MQTT_PUBLISH_TIMEOUT_MS);
    //printf("Message with delivery token %d delivered\n", token);
    return rc;
}

int iotc_device_client_send_ack_message_qos(const char* message, int qos) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    pubmsg.payload = (void*)message;
    pubmsg.payloadlen = (int)strlen(message);
    pubmsg.qos = qos;
    pubmsg.retained = 0;
    if ((rc = MQTTClient_publishMessage(client, ack_topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
        return rc;
    }

    rc = MQTTClient_waitForCompletion(client, token, MQTT_PUBLISH_TIMEOUT_MS);
    //printf("Message with delivery token %d delivered\n", token);
    return rc;
}

int iotc_device_client_send_di_message_qos(const char* message, int qos) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    pubmsg.payload = (void*)message;
    pubmsg.payloadlen = (int)strlen(message);
    pubmsg.qos = qos;
    pubmsg.retained = 0;
    if ((rc = MQTTClient_publishMessage(client, di_cmd_topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
        return rc;
    }

    rc = MQTTClient_waitForCompletion(client, token, MQTT_PUBLISH_TIMEOUT_MS);
    //printf("Message with delivery token %d delivered\n", token);
    return rc;
}


int iotc_device_client_send_hb_message_qos(const char* message, int qos) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    pubmsg.payload = (void*)message;
    pubmsg.payloadlen = (int)strlen(message);
    pubmsg.qos = qos;
    pubmsg.retained = 0;
    if ((rc = MQTTClient_publishMessage(client, hb_pub_topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
        return rc;
    }
    rc = MQTTClient_waitForCompletion(client, token, MQTT_PUBLISH_TIMEOUT_MS);
    return rc;
}

int iotc_device_client_send_twin_message_qos(const char* message, int qos) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    pubmsg.payload = (void*)message;
    pubmsg.payloadlen = (int)strlen(message);
    pubmsg.qos = qos;
    pubmsg.retained = 0;
    if ((rc = MQTTClient_publishMessage(client, twin_rpt_topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
        return rc;
    }

    rc = MQTTClient_waitForCompletion(client, token, MQTT_PUBLISH_TIMEOUT_MS);
    //printf("Message with delivery token %d delivered\n", token);
    return rc;
}

int iotc_device_client_send_hb_message(const char* message) {
    return iotc_device_client_send_hb_message_qos(message, 1);
}

int iotc_device_client_send_message(const char* message) {
    return iotc_device_client_send_message_qos(message, 1);
}

int iotc_device_client_send_ack_message(const char* message) {
    return iotc_device_client_send_ack_message_qos(message, 1);
}

int iotc_device_client_send_twin_message(const char* message) {//edit twin
    return iotc_device_client_send_twin_message_qos(message, 1);
}

int iotc_device_client_send_di_message(const char* message) {//edit Device Identity
    return iotc_device_client_send_di_message_qos(message, 0);
}




int iotc_device_client_init(IotConnectDeviceClientConfig* c) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    int rc;

    paho_deinit(); // reset all locals


    publish_topic = strdup(c->sr->broker.pub_topic);
    if (!publish_topic) {
        fprintf(stderr, "ERROR: Unable to allocate memory for paho host URL!");
        return -1;
    }
    ack_topic = strdup(c->sr->broker.ack_pub_topic);
    if (!ack_topic)
    {
        fprintf(stderr, "ERROR: Unable to allocate memory for paho host URL!");
        return -1;
    }

    di_cmd_topic = strdup(c->sr->broker.di_pub);
    if (!di_cmd_topic)
    {
        fprintf(stderr, "ERROR: Unable to allocate memory for paho host URL!");
        return -1;
    }

    hb_pub_topic = strdup(c->sr->broker.hb_pub);
    if (!hb_pub_topic)
    {
        fprintf(stderr, "ERROR: Unable to allocate memory for paho host URL!");
        return -1;
    }





    subs_topic = strdup(c->sr->broker.sub_topic);
    if (!subs_topic)
    {
        fprintf(stderr, "ERROR: Unable to allocate memory for paho host URL!");
        return -1;
    }

    char* paho_host_url = malloc(sizeof(HOST_URL_FORMAT) - 2 + strlen(c->sr->broker.host));
    if (NULL == paho_host_url) {
        fprintf(stderr, "ERROR: Unable to allocate memory for paho host URL!");
        return -1;
    }
    sprintf(paho_host_url, HOST_URL_FORMAT, c->sr->broker.host);

    if ((rc = MQTTClient_create(&client, paho_host_url, c->sr->broker.client_id,
        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to create client, return code %d\n", rc);
        free(paho_host_url);
        return rc;
    }
    free(paho_host_url);

    if ((rc = MQTTClient_setCallbacks(client, NULL, on_connection_lost, on_c2d_message, NULL)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to set callbacks, return code %d\n", rc);
        paho_deinit();
        return rc;
    }

    /*
    if ((rc = MQTTClient_setCallbacks(client, NULL, on_connection_lost, on_twin_c2d_message, NULL)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to set twin callbacks, return code %d\n", rc);
        paho_deinit();
        return rc;
    }*/

    ssl_opts.verify = 1;
    ssl_opts.trustStore = c->auth->trust_store;
    if (c->auth->type == IOTC_AT_X509) {
        ssl_opts.keyStore = c->auth->data.cert_info.device_cert;
        ssl_opts.privateKey = c->auth->data.cert_info.device_key;
    }
    conn_opts.ssl = &ssl_opts;

    status_cb = c->status_cb;
    conn_opts.username = c->sr->broker.user_name;
    conn_opts.password = c->sr->broker.pass;
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        paho_deinit();
        return rc;
    }

    is_initialized = true; // even if we fail below, we are ok

    if ((rc = MQTTClient_subscribe(client, c->sr->broker.sub_topic, 1)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to subscribe to c2d topic, return code %d\n", rc);
        rc = -2;
    }
    c2d_msg_cb = c->c2d_msg_cb;

    if ((rc = MQTTClient_subscribe(client, twin_dis_topic, 1)) != MQTTCLIENT_SUCCESS)//edit twin
    {
        printf("Failed to subscribe to c2d twin topic, return code %d\n", rc);
        rc = -2;
    }

    c2d_twin_cb = c->c2d_twin_cb; // edit twin


    if (status_cb) {
        status_cb(IOTC_CS_MQTT_CONNECTED);
    }

    return rc;
}

// not supported for PAHO
char* iotc_device_client_get_tpm_registration_id() {
    return NULL;
}

