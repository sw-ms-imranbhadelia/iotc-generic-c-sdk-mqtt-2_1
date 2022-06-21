//
// Copyright: Avnet, Softweb Inc. 2020
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//

#ifndef IOTCONNECT_H
#define IOTCONNECT_H

#include <stddef.h>
#include <time.h>
#include "iotconnect_event.h"
#include "iotconnect_telemetry.h"
#include "iotconnect_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        char* twinKey;
        void* twinVar;
        int* twinFd;
        bool active_high;
    } twin_t;

    typedef enum {
        // Authentication based on your CPID. Sync HTTP endpoint returns a long lived SAS token
        // This auth type is only intended as a simple way to connect your test and development devices
        // and must not be used in production
        IOTC_AT_TOKEN = 1,

        // CA Cert and Self Signed Cert
        IOTC_AT_X509 = 2,

        // TPM hardware devices
        IOTC_AT_TPM = 4, // 4 for compatibility with sync

        // IoTHub Key based authentication with Symmetric Keys (Primary or Secondary key)
        IOTC_AT_SYMMETRIC_KEY = 5

    } IotConnectAuthType;

    typedef enum {
        IOTC_CS_UNDEFINED,
        IOTC_CS_MQTT_CONNECTED,
        IOTC_CS_MQTT_DISCONNECTED
    } IotConnectConnectionStatus;

    typedef void (*IotConnectStatusCallback)(IotConnectConnectionStatus data);
    //typedef void (*IotConnectTwinC2dCallback)(const char* message, size_t message_len);//edit twin
    typedef void (*IotConnectC2dCallback)(unsigned char* message, size_t message_len);

    typedef struct {
        IotConnectAuthType type;
        char* trust_store; // Path to a file containing the trust certificates for the remote MQTT host
        union {
            struct {
                char* device_cert; // Path to a file containing the device CA cert (or chain) in PEM format
                char* device_key; // Path to a file containing the device private key in PEM format
            } cert_info;
            char* symmetric_key;
            char* scope_id; // for TPM authentication. AKA: ID Scope
        } data;
    } IotConnectAuthInfo;

    typedef struct {
        char* env;    // Settings -> Key Vault -> CPID.
        char* cpid;   // Settings -> Key Vault -> Evnironment.
        char* duid;   // Name of the device.
        char* sid;     //edit
        int qos; // QOS for outbound messages. Default 1.
        IotConnectAuthInfo auth_info;
        IotclOtaCallback ota_cb; // callback for OTA events.
        IotclModCallback mod_cb;
        IotclCommandCallback cmd_cb; // callback for command events.

        IotclgetattCallback  getatt_cb;
        IotclgetchCallback get_ch;
        IotclRespReceCallback resp_recive;
        IotclgettwinnCallback gettwin_cb;

        IotclRuleChCallback rule_cb;

        IotcltwinrecivCallback twin_msg_rciv;


        IotclMessageCallback msg_cb; // callback for ALL messages, including the specific ones like cmd or ota callback.
        IotConnectC2dCallback c2d_msg_cb;
        IotConnectStatusCallback status_cb; // callback for connection status
    } IotConnectClientConfig;

    typedef struct Sync_Resp {

        char *dvc_ID[10];
        char* tag[10];

    };
    struct Sync_Resp SYNC_resp;


    IotConnectClientConfig* iotconnect_sdk_init_and_get_config();

    // call iotconnect_sdk_init_and_get_config first and configure the SDK before calling iotconnect_sdk_init()
    int iotconnect_sdk_init();

    bool iotconnect_sdk_is_connected();

    // Can be used to pass to telemetry functions
    IotclConfig* iotconnect_sdk_get_lib_config();

    // Will check if there are inbound messages and call adequate callbacks if there are any
    // This is technically not required for the Paho implementation.
    void iotconnect_sdk_receive();

    // blocks until sent and returns 0 if successful.
    // data is a null-terminated string
    int iotconnect_sdk_send_packet(const char* data);

    int iotconnect_sdk_send_packet_test(const char* data);


    int iotconnect_sdk_send_hb_packet(const char* data);

    int iotconnect_sdk_send_ack_packet(const char* data);//Edit

    int iotconnect_sdk_send_twin_packet(const char* data);//edit twin

    int iotconnect_sdk_send_di_packet(const char* data);//edit Device Identity

    void updateTwin(const char* key, const char* value);//edit twin

    void updateTwin_test(const char* key, void* value, int type);

    void updateTwin_int(const char* key, int value);

    void updateTwin_bool(const char* key, bool value);

    void updateTwin_string(const char* key, const char* value);

    void updateTwin_float(const char* key, double value);

    char* iotcl_process_twin_event(const char* event, const char* twin_user_key);//edit twin

    //const char* time_func();

    char* sync_tg();

    int sync_d_pro();

    //void getattributes(IotclEventData command);

    void iotconnect_sdk_disconnect();

    void Dispose();

#ifdef __cplusplus
}
#endif

#endif
