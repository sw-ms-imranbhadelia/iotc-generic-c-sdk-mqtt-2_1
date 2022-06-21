//
// Copyright: Avnet 2021
// Created by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//

#ifndef IOTC_PAHO_CLIENT_H
#define IOTC_PAHO_CLIENT_H

#include "iotconnect_discovery.h"
#include "iotconnect.h"

#ifdef __cplusplus
extern   "C" {
#endif


    typedef void (*IotConnectC2dCallback)(unsigned char* message, size_t message_len);
    typedef void (*IotConnectTwinC2dCallback)(const char* message, size_t message_len);//edit twin
    //typedef void (*IotConnectChildDevCallback)(unsigned char* message);

    typedef struct {
        int qos; // default QOS is 1
        IotclSyncResponse* sr;
        IotConnectAuthInfo* auth; // Pointer to IoTConnect auth configuration
        IotConnectC2dCallback c2d_msg_cb; // callback for inbound messages
        IotConnectTwinC2dCallback c2d_twin_cb;//edit twin
        //IotConnectChildDevCallback child_dev;
        IotConnectStatusCallback status_cb; // callback for connection status
    } IotConnectDeviceClientConfig;

    int iotc_device_client_init(IotConnectDeviceClientConfig* c);

    int iotc_device_client_disconnect();

    bool iotc_device_client_is_connected();

    // sends message with QOS 1
    int iotc_device_client_send_message(const char* message);

    int iotc_device_client_send_hb_message(const char* message);

    // sends ack message with QOS 1
    int iotc_device_client_send_ack_message(const char* message);

    // sends twin message with QOS 1
    int iotc_device_client_send_twin_message(const char* message);//edit twin

    // sends di message with QOS 1
    int iotc_device_client_send_di_message(const char* message);


    // sends message with specified qos
    int iotc_device_client_send_message_qos(const char* message, int qos);

    // sends ack message with specified qos
    int iotc_device_client_send_ack_message_qos(const char* message, int qos);

    // sends Heart Beat message with specified qos
    int iotc_device_client_send_hb_message_qos(const char* message, int qos);

    // sends twin reporting message with specified qos
    int iotc_device_client_send_twin_message_qos(const char* message, int qos);



    // sends device Identity message with specified qos
    int iotc_device_client_send_di_message_qos(const char* message, int qos);

    void iotc_device_client_receive();

    // Returns the TPM registration ID from the TPM chip.
    // Not supported in Paho Client. It will always return null.
    // The returned value MUST be freed.
    char* iotc_device_client_get_tpm_registration_id();

#ifdef __cplusplus
}
#endif

#endif // IOTC_PAHO_CLIENT_H
