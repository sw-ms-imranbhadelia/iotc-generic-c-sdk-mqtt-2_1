//
// Copyright: Avnet 2020
// Created by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include<time.h>

#include "iotconnect_common.h"
#include "iotconnect.h"

#include "app_config.h"

// windows compatibility

#if defined(_WIN32) || defined(_WIN64)
#define F_OK 0
#include <Windows.h>
#include <io.h>
int usleep(unsigned long usec) {
    Sleep(usec / 1000);
    return 0;
}
#define access    _access_s
#else
#include <unistd.h>
#endif


#define APP_VERSION "00.01.00"


static void delay(int number_of_seconds)
{
    int milli_seconds = 1000 * number_of_seconds;
    clock_t start_time = clock();

    while (clock() < start_time + milli_seconds);
}

static void ConnectionStatusChangedCallback(IotConnectConnectionStatus status) {
    // Add your own status handling
    switch (status) {
    case IOTC_CS_MQTT_CONNECTED:
        printf("IoTConnect Client Connected\n");
        break;
    case IOTC_CS_MQTT_DISCONNECTED:
        printf("IoTConnect Client Disconnected\n");
        break;
    default:
        printf("IoTConnect Client ERROR\n");
        break;
    }
}

static void command_status(IotclEventData data, bool status, const char* command_name, const char* message) {
    const char* ack = iotcl_create_ack_string_and_destroy_event(data, status, message);
    printf("command: %s status=%s: %s\n", command_name, status ? "OK" : "Failed", message);
    printf("Sent CMD ack: %s\n", ack);
    //iotconnect_sdk_send_packet(ack);//Re-edit
    iotconnect_sdk_send_ack_packet(ack);
    free((void*)ack);
}



static void onDeviceCommand(IotclEventData data) {
    char* command = iotcl_clone_command(data);
    if (NULL != command) {
        command_status(data, true, command, "Not implemented");
        free((void*)command);
    }
    else {
        command_status(data, false, "?", "Internal error");
    }
}



static bool is_app_version_same_as_ota(const char* version) {
    return strcmp(APP_VERSION, version) == 0;
}

static bool app_needs_ota_update(const char* version) {
    return strcmp(APP_VERSION, version) < 0;
}

static void gettwin(IotclEventData command)
{
    const char* get_twin = prosess_cmd(command);
    printf("Send get twin cmd %s\n", get_twin);
    iotconnect_sdk_send_di_packet(get_twin);
    free((void*)get_twin);
}

static void onRuleChangeCommand(IotclEventData command)
{
    const char* rule_ch = prosess_cmd(command);
    printf("Send get twin cmd %s\n", rule_ch);
    printf(" get rule cmd\n");

}
static void GetChildDevice(IotclEventData command)
{
    const char* getchild = prosess_cmd(command);
    printf("Send get child cmd %s\n", getchild);
    iotconnect_sdk_send_di_packet(getchild);
    free((void*)getchild);
}



static void getattributes(IotclEventData command)
{
    const char* get_att_str = prosess_cmd(command);
    printf("Send get attribute cmd %s\n", get_att_str);
    iotconnect_sdk_send_di_packet(get_att_str);
    free((void*)get_att_str);
}



static void response_recive(IotclEventData root, IotConnectEventType type)
{
    const char* response = response_string(root, type);
    printf(":%s\n", response);
    free((void*)response);
}

static void twin_collback(IotclEventData payload)
{
    char* key = NULL;
    char *value = NULL;
    //void * value = NULL ;
    int device_type;
    printf("\n Twin_msg payload is >>  %s", payload);

    cJSON* root = cJSON_Parse(payload);
    cJSON* D = cJSON_GetObjectItem(root, "desired");
    if (D) {
        cJSON* device = D->child;
        while (device) {
            if (!strcmp(device->string, "$version")) {

            }
            else 
            {
                key = device->string;
                device_type = device->type;
                if (device_type == 2) {
                    bool X = NULL;
                    int  int_val = (cJSON_GetObjectItem(D, key))->valueint;
                    printf("\nint value: %d", int_val);
                    if (int_val == 1){
                        X = true;
                    }
                    updateTwin_bool(key, X);
                    
                }
                if (device_type == 1){
                    bool X =  NULL;
                    int  int_val = (cJSON_GetObjectItem(D, key))->valueint;
                    printf("\nint value: %d", int_val);
                    if (int_val == 0){
                        X = false;
                    }
                    updateTwin_bool(key, X);

                }
                if(device_type == 8){ 
                    int  int_val;
                    double diff, flot_val;
                    
                    flot_val = (cJSON_GetObjectItem(D, key))->valuedouble;
                    int_val = flot_val;
                    diff = flot_val - int_val;
                    if (diff > 0) { 
                        printf("\nfloat value: %f", (cJSON_GetObjectItem(D, key))->valuedouble);
                        updateTwin_float(key, flot_val);
                    } 
                    if (diff <= 0){
                        printf("\nint value: %d", (cJSON_GetObjectItem(D, key))->valueint);
                        updateTwin_int(key, int_val);
                    }
                }
                if (device_type == 16){
                    
                    //strcpy((char*)value, *(char*)(cJSON_GetObjectItem(D, key))->valuestring);
                    value = (cJSON_GetObjectItem(D, key))->valuestring;
                    printf("\nstring value: %s", value);
                    updateTwin_string(key, value);

                }
                if (device_type == 4 || device_type == 64)
                {
                    printf("\n Removed twin %s has value NULL\n", key);
                }
               
                
                /*
                 Type    : Public Method "updateTwin()"
                 Usage   : Upate the twin reported property
                 Output  :
                 Input   : "key" and "value" as below
                           // String key = "<< Desired property key >>"; // Desired proeprty key received from Twin callback message
                           // String value = "<< Desired Property value >>"; // Value of respective desired property
                */
               // updateTwin(key, value);
                //updateTwin_test(key, value, device_type);
            }
            device = device->next;
        }
    }
}

/*
static void twin_collback(IotclEventData payload)
{
    const char* twin_user_key = "StatusLED";
    const char* twin_string = NULL;
    twin_string = iotcl_process_twin_event(payload, twin_user_key);

    updateTwin(twin_user_key, twin_string);
} */


static void onmodulecommand(IotclEventData data)
{
    char* mod_url = iotcl_clone_download_url(data, 0);
    printf("Download URL is: %s\n", mod_url);
}

static void onotacommand(IotclEventData data)
{
    const char* message = NULL;
    char* url = iotcl_clone_download_url(data, 0);
    bool success = false;
    if (NULL != url) {
        printf("Download URL is: %s\n", url);
        const char* version = iotcl_clone_sw_version(data);
        if (is_app_version_same_as_ota(version))
        {
            printf("OTA request for same version %s. Sending success\n", version);
            success = true;
            message = "Version is matching";
        }
        else if (app_needs_ota_update(version))
        {
            printf("OTA update is required for version %s.\n", version);
            success = true;
            message = "Not implemented";
        }
        else
        {
            printf("Device firmware version %s is newer than OTA version %s. Sending failure\n", APP_VERSION,
                version);
            // Not sure what to do here. The app version is better than OTA version.
            // Probably a development version, so return failure?
            // The user should decide here.
            success = false;
            message = "Device firmware version is newer";
        }

        free((void*)url);
        free((void*)version);
    }
    else
    {
        // compatibility with older events
        // This app does not support FOTA with older back ends, but the user can add the functionality
        const char* command = iotcl_clone_command(data);
        if (NULL != command)
        {
            // URL will be inside the command
            printf("Command is: %s\n", command);
            message = "Old back end URLS are not supported by the app";
            free((void*)command);
        }
    }
    const char* ack = iotcl_create_ack_string_and_destroy_event(data, success, message);
    if (NULL != ack)
    {
        printf("Sent OTA ack: %s\n", ack);
        //iotconnect_sdk_send_packet(ack);//Re-edit
        iotconnect_sdk_send_ack_packet(ack);
        free((void*)ack);
    }
}


static void publish_telemetry() {
    IotclMessageHandle msg = iotcl_telemetry_create(iotconnect_sdk_get_lib_config());
    cJSON* Gy;
    // Optional. The first time you create a data point, the current timestamp will be automatically added
    // TelemetryAddWith* calls are only required if sending multiple data points in one packet.
    iotcl_telemetry_add_with_iso_time(msg, iotcl_iso_timestamp_now());
    iotcl_telemetry_set_string(msg, "version", APP_VERSION);
    iotcl_telemetry_set_number(msg, "cpu", 3.123); // test floating point numbers


    const char* str = iotcl_create_serialized_string(msg, false);//

    printf("pub_telemetry : %s\n", str);
    //const char* str = "{\"dt\":\"2022-01-05T09:42:33.000Z\",\"d\":[{\"id\":\"stm32-demo\",\"tg\":\"\",\"dt\":\"2022-01-05T09:42:34.000Z\",\"d\":{\"version\":\"00.01.00\",\"cpu\": 3.123}}]}";
    //iotcl_telemetry_destroy(msg);
    //iotconnect_sdk_send_packet(str); // underlying code will report an error
    //iotcl_destroy_serialized(str);
}

void ReadSensordata(void) {

    char* NowTime = iotcl_iso_timestamp_now(); 	// time format : YYYY-MM-DDTHH:MM:SS.FFFZ eg. 2020-01-21T22:57:59.968Z
    char* sensors_data = NULL;
    cJSON* obj, *obj2, *obj3, * root_array, * data, * data2, * data3, * Gy;
     // For Gateway Device
          /**
       * Gateway device input data format Example:
       * For Gateway and multiple child device
       *	String data = [{
       *		"uniqueId": "<< Gateway Device UniqueId >>", // It should be must first object of the array
       *		"time": "<< date >>",
       *		"data": {}
       *	},
       *	{
       *		"uniqueId":"<< Child DeviceId >>", //Child device
       *		"time": "<< date >>",
       *		"data": {}
       *	}]
       *
       */
       root_array = cJSON_CreateArray();
       obj = cJSON_CreateObject();

       cJSON_AddItemToArray(root_array,obj);

       cJSON_AddStringToObject(obj, "uniqueId", IOTCONNECT_DUID);
       cJSON_AddStringToObject(obj, "time", NowTime);
       cJSON_AddItemToObject(obj, "data", data=cJSON_CreateObject());
       cJSON_AddStringToObject(data, "version", APP_VERSION);
       cJSON_AddNumberToObject(data, "cpu", 3.123);
       /*
       	obj2 = cJSON_CreateObject();
       	cJSON_AddItemToArray(root_array,obj2);
       	cJSON_AddStringToObject(obj2, "uniqueId","windemoch1");
       	cJSON_AddStringToObject(obj2, "time", NowTime);
       	cJSON_AddItemToObject(obj2, "data", data2=cJSON_CreateObject());
       	cJSON_AddNumberToObject(data2, "Temperature",56);
       	cJSON_AddStringToObject(data2, "colour", "red");

        obj3 = cJSON_CreateObject();
        cJSON_AddItemToArray(root_array, obj3);
        cJSON_AddStringToObject(obj3, "uniqueId", "windemoch2");
        cJSON_AddStringToObject(obj3, "time", NowTime);
        cJSON_AddItemToObject(obj3, "data", data3 = cJSON_CreateObject());
        cJSON_AddItemToObject(data3, "Gyroscope", Gy = cJSON_CreateObject());
        cJSON_AddNumberToObject(Gy, "X", 44);
        cJSON_AddNumberToObject(Gy, "Y", 33);
        cJSON_AddNumberToObject(Gy, "Z", 55);
        */
        sensors_data = cJSON_Print(root_array);
       // printf(" dummy %s\n", sensors_data);
        //cJSON_Delete(root);
        iotcl_telemetry_destroy(root_array);
        iotconnect_sdk_send_packet_test(sensors_data); // underlying code will report an error
        iotcl_destroy_serialized(sensors_data);
}

int main(int argc, char* argv[])
{
    if (access(IOTCONNECT_SERVER_CERT, F_OK) != 0) {
        fprintf(stderr, "Unable to access IOTCONNECT_SERVER_CERT. "
            "Please change directory so that %s can be accessed from the application or update IOTCONNECT_CERT_PATH\n",
            IOTCONNECT_SERVER_CERT);
    }

    if (IOTCONNECT_AUTH_TYPE == IOTC_AT_X509) {
        if (access(IOTCONNECT_IDENTITY_CERT, F_OK) != 0 ||
            access(IOTCONNECT_IDENTITY_KEY, F_OK) != 0
            ) {
            fprintf(stderr, "Unable to access device identity private key and certificate. "
                "Please change directory so that %s can be accessed from the application or update IOTCONNECT_CERT_PATH\n",
                IOTCONNECT_SERVER_CERT);
        }
    }

    IotConnectClientConfig* config = iotconnect_sdk_init_and_get_config();
    config->cpid = IOTCONNECT_CPID;
    config->env = IOTCONNECT_ENV;
    config->duid = IOTCONNECT_DUID;
    config->sid = IOTCONNECT_SID;//Edit
    config->auth_info.type = IOTCONNECT_AUTH_TYPE;
    config->auth_info.trust_store = IOTCONNECT_SERVER_CERT;

    if (config->auth_info.type == IOTC_AT_X509) {
        config->auth_info.data.cert_info.device_cert = IOTCONNECT_IDENTITY_CERT;
        config->auth_info.data.cert_info.device_key = IOTCONNECT_IDENTITY_KEY;
    }
    else if (config->auth_info.type == IOTC_AT_TPM) {
        config->auth_info.data.scope_id = IOTCONNECT_SCOPE_ID;
    }
    else if (config->auth_info.type == IOTC_AT_SYMMETRIC_KEY) {
        config->auth_info.data.symmetric_key = IOTCONNECT_SYMMETRIC_KEY;
    }
    else if (config->auth_info.type != IOTC_AT_TOKEN) { // token type does not need any secret or info
     // none of the above
        fprintf(stderr, "IOTCONNECT_AUTH_TYPE is invalid\n");
        return -1;
    }


    config->status_cb = ConnectionStatusChangedCallback;
    config->ota_cb = onotacommand;
    config->cmd_cb = onDeviceCommand;
    config->mod_cb = onmodulecommand;
    config->getatt_cb = getattributes;
    config->get_ch = GetChildDevice;
    config->rule_cb = onRuleChangeCommand;
    config->twin_msg_rciv = twin_collback;
    config->resp_recive = response_recive;
    config->gettwin_cb = gettwin;

    // run a dozen connect/send/disconnect cycles with each cycle being about a minute
    for (int j = 0; j < 10; j++) {
        int ret = iotconnect_sdk_init();
        if (0 != ret) {
            fprintf(stderr, "IoTConnect exited with error code %d\n", ret);
            return ret;
        }
        //GetChildDevice(204);
        //getattributes(201);
        //gettwin(202);
        //createChildDevice("windemoch3","ch1","windemoch3");
        //deleteChildDevice("windemoch3");
        // send 10 messages
        for (int i = 0; iotconnect_sdk_is_connected() && i < 10000; i++) 
        {
            ReadSensordata();
            //publish_telemetry();
            // repeat approximately evey ~5 seconds
            for (int k = 0; k <1; k++) 
            {
                iotconnect_sdk_receive();
                delay(1);
                //usleep(1000); // 10ms
            }
        }
        iotconnect_sdk_disconnect();
    }
    Dispose();
    return 0;
}
