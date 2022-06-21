/* Copyright (C) 2020 Avnet - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

#include "iotconnect_common.h"
#include "iotconnect_lib.h"
 //#include "../../cJSON/cJSON.h"

#define CJSON_ADD_ITEM_HAS_RETURN \
    (CJSON_VERSION_MAJOR * 10000 + CJSON_VERSION_MINOR * 100 + CJSON_VERSION_PATCH >= 10713)

#if !CJSON_ADD_ITEM_HAS_RETURN
#error "cJSON version must be 1.7.13 or newer"
#endif

struct IotclEventDataTag {
    char* key;
    char* value;
    char* payload;
    int command;
    cJSON* data;
    cJSON* root;
    IotConnectEventType type;
};

/*
Conversion of boolean to IOT 2.0 specification messages:

For commands:
Table 15 [Possible values for st]
4 Command Failed with some reason
6 Executed successfully

For OTA:
Table 16 [Possible values for st]
4 Firmware command Failed with some reason
7 Firmware command executed successfully
*/
static int to_ack_status(bool success, IotConnectEventType type) {
    int status = 4; // default is "failure"
    if (success == true)
    {
        switch (type)
        {
        case DEVICE_COMMAND:
            status = 6;
            break;
        case DEVICE_OTA:
            status = 0;
            break;
        default:; // Can't do more than assume failure if unknown type is used.
        }
    }
    return status;
}


static bool iotc_process_callback(struct IotclEventDataTag* eventData)
{
    if (!eventData)
    {
        return false;
    }
    IotclConfig* config = iotcl_get_config();
    if (!config)
    {
        return false;
    }

    switch (eventData->type)
    { //DF_CHANGE

    case ON_FORCE_SYNC:
        if (config->event_functions.msg_cb)
        {
            config->event_functions.msg_cb(eventData, eventData->type);
        }
        break;
    case DF_CHANGE:
        if (config->event_functions.get_df)
        {
            config->event_functions.get_df(eventData);
        }
        break;

    case HEARTBEAT_COMMAND:
        if (config->event_functions.hb_cmd)
        {
            config->event_functions.hb_cmd(eventData);
        }
        break;

    case STOP_HEARTBEAT:
        if (config->event_functions.hb_stop)
        {
            config->event_functions.hb_stop(eventData);
        }
        break;

    case DEVICE_COMMAND:
        if (config->event_functions.cmd_cb)
        {
            config->event_functions.cmd_cb(eventData);
        }
        break;
    case ON_CHANGE_ATTRIBUTE:
        if (config->event_functions.getatt_cb)
        {
            int cmmd = 201;
            config->event_functions.getatt_cb(cmmd);
        }
        break;

    case REFRESH_CHILD_DEVICE:
        if (config->event_functions.get_ch)
        {
            int ch_cmd = 204;

            config->event_functions.get_ch(ch_cmd);
        }
        break;

    case RULE_CHANGE_COMMAND://
        if (config->event_functions.rule_cb)
        {
            int cmmd = 203;
            config->event_functions.rule_cb(cmmd);
        }
        break;

    case REFRESH_SETTING_TWIN:
        if (config->event_functions.gettwin_cb)
        {
            int cmmd = 202;
            config->event_functions.gettwin_cb(cmmd);
        }
        break;

    case ON_CLOSE:
        if (config->event_functions.msg_cb)
        {
            config->event_functions.msg_cb(eventData, eventData->type);
        }
        break;

    case DEVICE_DELETE:
        if (config->event_functions.msg_cb)
        {
            config->event_functions.msg_cb(eventData, eventData->type);
        }
        break;

    case DEVICE_DISABLE:
        if (config->event_functions.msg_cb)
        {
            config->event_functions.msg_cb(eventData, eventData->type);
        }
        break;

    case STOP_OPERATION:
        if (config->event_functions.msg_cb)
        {
            config->event_functions.msg_cb(eventData, eventData->type);
        }
        break;


    case 201:
        if (config->event_functions.resp_recive)
        {

            config->event_functions.resp_recive(eventData, eventData->type);
        }
        break;

    case 202:
        if (config->event_functions.resp_recive)
        {

            config->event_functions.resp_recive(eventData, eventData->type);
        }
        break;
    case 204:
        if (config->event_functions.resp_recive)
        {
            config->event_functions.child_dev(eventData);
            //config->event_functions.resp_recive(eventData, eventData->type);
        }
        break;

    case DEVICE_OTA:
        if (config->event_functions.ota_cb) {
            config->event_functions.ota_cb(eventData);
        }
        break;

    case  MODULE_UPDATE_COMMAND:
        if (config->event_functions.mod_cb) {
            config->event_functions.mod_cb(eventData);
        }
        break;
    default:
        break;
    }

    return true;
}


/************************ CJSON IMPLEMENTATION **************************/
static inline bool is_valid_string(const cJSON* json) {
    return (NULL != json && cJSON_IsString(json) && json->valuestring != NULL);
}


bool twin_event(const char* event)
{
    cJSON* root, * root2;
    char* SMS;
    IotclConfig* config = iotcl_get_config();
    if (!config)
    {
        return false;
    }
    printf("Twin recieve: %s \n", event);
    struct IotclEventDataTag* eventData = (struct IotclEventDataTag*)calloc(sizeof(struct IotclEventDataTag), 1);
    if (NULL == eventData) goto cleanup;

    root = cJSON_CreateObject();
    root2 = cJSON_Parse(event);
    cJSON_AddItemToObject(root, "desired", root2);
    //cJSON_AddStringToObject(root, "uniqueId", );
    SMS = cJSON_PrintUnformatted(root);

    eventData->payload = SMS;
    config->event_functions.twin_msg_rciv(eventData->payload);

    return true;

cleanup:
    return false;


}

/*
bool iotcl_process_twin_event(const char* event)
{
    const char* Twin_key;
    const char* Twin_value;
    IotclConfig* config = iotcl_get_config();
    if (!config)
    {
        return false;
    }


    printf("Twin recieve: %s \n", event);
    cJSON* rootProperties = cJSON_Parse(event);
    if (!rootProperties)
    {
        printf("WARNING: Cannot parse the string as JSON content.\n");
        return false;
    }

    cJSON* desiredProperties = cJSON_GetObjectItem(rootProperties, "desired");
    if (!desiredProperties)
    {
        desiredProperties = rootProperties;
    }

    cJSON* key = cJSON_GetObjectItemCaseSensitive(desiredProperties, "StatusLED");
    if (key)
    {
        Twin_key = "StatusLED";
        Twin_value = key->valuestring;



        config->event_functions.twin_up_cb(Twin_key, Twin_value);
    }


}*/

bool iotcl_process_event(const char* event)
{
    int cmd_type;
    int get_attr;
    cJSON* j_type = NULL;
    cJSON* dproperty = NULL;
    cJSON* cmd = NULL;
    cJSON* j_ack_id = NULL;

    cJSON* root = cJSON_Parse(event);
    if (!root)
        return false;

    {
        cJSON* j_type = cJSON_GetObjectItemCaseSensitive(root, "ct");
        if (j_type)
        {
            cmd_type = j_type->valueint;
            switch (cmd_type)
            {
            case DEVICE_COMMAND://ct :0
                cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
                if (!cmd)
                {
                    goto cleanup;
                }
                j_ack_id = cJSON_GetObjectItemCaseSensitive(root, "ack");
                if (!is_valid_string(j_ack_id))
                {
                    goto cleanup;
                }
                break;

            case DEVICE_OTA://ct : 1
                cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
                if (!cmd)
                {
                    goto cleanup;
                }

                j_ack_id = cJSON_GetObjectItemCaseSensitive(root, "ack");
                if (!is_valid_string(j_ack_id))
                {
                    goto cleanup;
                }
                break;

            case MODULE_UPDATE_COMMAND://ct : 2
                /*cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
                if (!cmd)
                {
                    goto cleanup;
                }

                j_ack_id = cJSON_GetObjectItemCaseSensitive(root, "ack");
                if (!is_valid_string(j_ack_id))
                {
                    goto cleanup;
                }*/
                break;


            case ON_CHANGE_ATTRIBUTE://ct :101
                get_attr = 201;
                break;

            case REFRESH_CHILD_DEVICE:

                break;

            case REFRESH_SETTING_TWIN://ct :102

                break;

            case ON_CLOSE://ct :108 

                break;

            case DEVICE_DELETE://ct :106 

                break;

            case DEVICE_DISABLE://ct :107  Device_connection_status

                break;

            case  Device_connection_status://ct :116 

                break;

            case STOP_OPERATION://ct :109 STOP_OPERATION DF_CHANGE

                break;

            case DF_CHANGE://ct :105  

                break;


            case HEARTBEAT_COMMAND://ct :110 START_HEARTBEAT

                break;

            case STOP_HEARTBEAT://ct :111 STOP_HEARTBEAT

                break;

            default:; // Can't do more than assume failure if unknown type is used.
            }

            IotConnectEventType type = cmd_type;

            struct IotclEventDataTag* eventData = (struct IotclEventDataTag*)calloc(sizeof(struct IotclEventDataTag), 1);
            if (NULL == eventData) goto cleanup;

            eventData->root = root;
            eventData->data;
            eventData->type = type;
            //eventData->command ;
            return iotc_process_callback(eventData);
        }

        cJSON* dProperties = cJSON_GetObjectItemCaseSensitive(root, "d");
        if (dProperties)
        {
            cJSON* ct = cJSON_GetObjectItemCaseSensitive(dProperties, "ct");
            cmd_type = ct->valueint;

            switch (cmd_type)
            {
            case 201://ct :101
                printf("Attributes Received ");
                break;
            case 202://ct :102
                printf("Twin Received");
                break;
            case 204://ct :104
                printf("Child Received\n");

                break;

            default:;
            }

            IotConnectEventType type = cmd_type;

            struct IotclEventDataTag* eventData = (struct IotclEventDataTag*)calloc(sizeof(struct IotclEventDataTag), 1);
            if (NULL == eventData) goto cleanup;

            eventData->root = root;
            eventData->data;
            eventData->type = type;
            //eventData->command ;
            return iotc_process_callback(eventData);
        }


    }

cleanup:
    if (root)
    {
        cJSON_free(root);
    }
    return false;
}


char* iotcl_clone_command(IotclEventData data) {
    //cJSON* command = cJSON_GetObjectItemCaseSensitive(data->data, "cmd");
    printf("data event %s\n", data->root);
    cJSON* command = cJSON_GetObjectItem(data->root, "cmd");
    if (NULL == command || !is_valid_string(command)) {
        return NULL;
    }

    return iotcl_strdup(command->valuestring);
}


char* iotcl_clone_download_url(IotclEventData data, size_t index) {
    cJSON* urls = cJSON_GetObjectItemCaseSensitive(data->root, "urls");
    if (NULL == urls || !cJSON_IsArray(urls)) {
        return NULL;
    }
    if ((size_t)cJSON_GetArraySize(urls) > index) {
        cJSON* url = cJSON_GetArrayItem(urls, index);
        if (is_valid_string(url)) {
            return iotcl_strdup(url->valuestring);
        }
        else if (cJSON_IsObject(url)) {
            cJSON* url_str = cJSON_GetObjectItem(url, "url");
            if (is_valid_string(url_str)) {
                return iotcl_strdup(url_str->valuestring);
            }
        }
    }
    return NULL;
}


char* iotcl_clone_sw_version(IotclEventData data)
{
    /*cJSON* ver = cJSON_GetObjectItemCaseSensitive(data->root, "ver");
    if (cJSON_IsObject(ver))
    {
        cJSON* sw = cJSON_GetObjectItem(ver, "sw");
        if (is_valid_string(sw))
        {
            return iotcl_strdup(sw->valuestring);
        }
    }*///Re-edit
    cJSON* sw = cJSON_GetObjectItem(data->root, "sw");
    if (is_valid_string(sw))
    {
        return iotcl_strdup(sw->valuestring);
    }
    return NULL;
}

char* iotcl_clone_hw_version(IotclEventData data) {
    cJSON* ver = cJSON_GetObjectItemCaseSensitive(data->data, "ver");
    if (cJSON_IsObject(ver)) {
        cJSON* sw = cJSON_GetObjectItem(ver, "hw");
        if (is_valid_string(sw)) {
            return iotcl_strdup(sw->valuestring);
        }
    }
    return NULL;
}

char* iotcl_clone_ack_id(IotclEventData data)
{
    cJSON* ackid = cJSON_GetObjectItemCaseSensitive(data->data, "ackId");
    if (is_valid_string(ackid))
    {
        return iotcl_strdup(ackid->valuestring);
    }
    return NULL;
}

int hb_update(IotclEventData data)
{
    int ret = NULL;
    cJSON* hb = cJSON_GetObjectItemCaseSensitive(data->root, "f");
    ret = hb->valueint;
    return ret;
}

int hb_event(IotclEventData data)
{
    int ret = NULL;
    cJSON* hbct = cJSON_GetObjectItemCaseSensitive(data->root, "ct");
    ret = hbct->valueint;

    return ret;
}

int df_update(IotclEventData data)
{
    int ret = NULL;
    cJSON* df = cJSON_GetObjectItemCaseSensitive(data->root, "df");
    ret = df->valueint;

    return ret;
}

char* id_tg(IotclEventData data) {

    char* res = NULL;
    res = cJSON_PrintUnformatted(data->root);
    if (!res)goto cleanup;
cleanup:
    cJSON_Delete(data->root);
    return res;
}

char* response_string(IotclEventData root, IotConnectEventType type)
{

    char* result = NULL;

    result = cJSON_PrintUnformatted(root->root);

    if (!result)goto cleanup;

cleanup:
    cJSON_Delete(root->root);
    return result;

}

char* prosess_hb()
{
    char* result = NULL;

    cJSON* hb_root = cJSON_CreateObject();
    if (!hb_root)
    {
        return NULL;
    }
    result = cJSON_PrintUnformatted(hb_root);

cleanup:
    cJSON_Delete(hb_root);
    return result;
}

char* prosess_cmd(int command)
{
    //{ "mt": 201 }
    char* result = NULL;

    cJSON* att_root = cJSON_CreateObject();
    if (!att_root)
    {
        return NULL;
    }

    if (!cJSON_AddNumberToObject(att_root, "mt", command)) goto cleanup;

    result = cJSON_PrintUnformatted(att_root);

cleanup:
    cJSON_Delete(att_root);
    return result;

}

char* create_twin_json(
    const char* key,
    const char* value
)
{


    char* result = NULL;
    cJSON* twin_json = cJSON_CreateObject();

    if (!twin_json)
    {
        return NULL;
    }
    if (!cJSON_AddStringToObject(twin_json, key, value)) goto cleanup;
    result = cJSON_PrintUnformatted(twin_json);

cleanup:
    cJSON_Delete(twin_json);
    return result;

}


static char* create_ack(
    bool success,
    const char* message,
    IotConnectEventType message_type,
    const char* ack_id)
{

    char* result = NULL;

    IotclConfig* config = iotcl_get_config();

    if (!config) {
        return NULL;
    }

    cJSON* ack_json = cJSON_CreateObject();

    if (ack_json == NULL) {
        return NULL;
    }

    /*
    {
    "dt": "", //Current time stamp in format YYYY-MM-DDTHH:MM:SS.SSSZ
    "d": {
        "ack": "", //Acknowledgement GUID
        "type": 0, //Fixed – Acknowledgement of Command
        "st": 0, //Status of Acknowledgement Ref
        "msg": "", //Your custom message for acknowledgement
        "cid": "" //Applicable if ack is being sent for child device otherwise null **
    }
}
    */
    if (!cJSON_AddStringToObject(ack_json, "dt", iotcl_iso_timestamp_now())) goto cleanup;
    {
        cJSON* ack_data = cJSON_CreateObject();
        if (NULL == ack_data) goto cleanup;
        if (!cJSON_AddItemToObject(ack_json, "d", ack_data))
        {
            cJSON_Delete(ack_data);
            goto cleanup;
        }
        if (!cJSON_AddStringToObject(ack_data, "ack", ack_id)) goto cleanup;
        if (!cJSON_AddNumberToObject(ack_data, "type", message_type)) goto cleanup;
        if (!cJSON_AddNumberToObject(ack_data, "st", to_ack_status(success, message_type))) goto cleanup;
        if (!cJSON_AddStringToObject(ack_data, "msg", message ? message : "")) goto cleanup;

    }
    /*
    // message type 5 in response is the command response. Type 11 is OTA response.
    if (!cJSON_AddNumberToObject(ack_json, "mt", message_type == DEVICE_COMMAND ? 5 : 11)) goto cleanup;
    if (!cJSON_AddStringToObject(ack_json, "t", iotcl_iso_timestamp_now())) goto cleanup;

    if (!cJSON_AddStringToObject(ack_json, "uniqueId", config->device.duid)) goto cleanup;
    if (!cJSON_AddStringToObject(ack_json, "cpId", config->device.cpid)) goto cleanup;

    {
        cJSON* sdk_info = cJSON_CreateObject();
        if (NULL == sdk_info) {
            return NULL;
        }
        if (!cJSON_AddItemToObject(ack_json, "sdk", sdk_info)) {
            cJSON_Delete(sdk_info);
            goto cleanup;
        }
        if (!cJSON_AddStringToObject(sdk_info, "l", CONFIG_IOTCONNECT_SDK_NAME)) goto cleanup;
        if (!cJSON_AddStringToObject(sdk_info, "v", CONFIG_IOTCONNECT_SDK_VERSION)) goto cleanup;
        if (!cJSON_AddStringToObject(sdk_info, "e", config->device.env)) goto cleanup;
    }

    {
        cJSON* ack_data = cJSON_CreateObject();
        if (NULL == ack_data) goto cleanup;
        if (!cJSON_AddItemToObject(ack_json, "d", ack_data)) {
            cJSON_Delete(ack_data);
            goto cleanup;
        }
        if (!cJSON_AddStringToObject(ack_data, "ackId", ack_id)) goto cleanup;
        if (!cJSON_AddStringToObject(ack_data, "msg", message ? message : "")) goto cleanup;
        if (!cJSON_AddNumberToObject(ack_data, "st", to_ack_status(success, message_type))) goto cleanup;
    }
    */ // Re-edit

    result = cJSON_PrintUnformatted(ack_json);

    // fall through
cleanup:
    cJSON_Delete(ack_json);
    return result;
}

char* iotcl_create_ack_string_and_destroy_event(
    IotclEventData data,
    bool success,
    const char* message
) {
    if (!data) return NULL;
    // alrwady checked that ack ID is valid in the messages
    //char* ack_id = cJSON_GetObjectItemCaseSensitive(data->data, "ackId")->valuestring; //Re-edit
    char* ack_id = cJSON_GetObjectItemCaseSensitive(data->root, "ack")->valuestring;
    char* ret = create_ack(success, message, data->type, ack_id);
    iotcl_destroy_event(data);
    return ret;
}

char* iotcl_create_ota_ack_response(
    const char* ota_ack_id,
    bool success,
    const char* message
)
{
    char* ret = create_ack(success, message, DEVICE_OTA, ota_ack_id);
    return ret;
}

void iotcl_destroy_event(IotclEventData data)
{
    cJSON_Delete(data->root);
    free(data);
}

