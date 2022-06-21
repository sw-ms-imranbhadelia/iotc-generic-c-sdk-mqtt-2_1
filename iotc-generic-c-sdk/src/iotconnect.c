//
// Copyright: Avnet, Softweb Inc. 2021
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#ifdef IOTC_USE_PAHO
#include "iotc_algorithms.h"
#endif
#include "iotconnect_discovery.h"
#include "iotconnect.h"
#include "iotc_http_request.h"
#include "iotc_device_client.h"

#include "iotconnect_common.h"


//#define HTTP_DISCOVERY_URL_FORMAT "https://%s/api/sdk/cpid/%s/lang/M_C/ver/2.0/env/%s"//re-Edit old format 
#define HTTP_DISCOVERY_URL_FORMAT "http://%s/api/v2.1/dsdk/sid/%s" //"https://%s/api/sdk/cpid/%s/lang/M_C/ver/2.0/env/%s" // Edit
//#define HTTP_SYNC_URL_FORMAT "https://%s%ssync?"//Re-edit


#define HTTP_SYNC_URL_FORMAT "%s/uid/%s"//edit 
 
// Define the signature for the device twin function pointer
// We need to use the void* to avoid a cicular reference in the twin_t struct
//typedef void (*dtHandler)(void*, cJSON*);

static IotclConfig lib_config = { 0 };
static IotConnectClientConfig config = { 0 };

// cached discovery/sync response:
static IotclDiscoveryResponse* discovery_response = NULL;
static IotclSyncResponse* sync_response = NULL;

//static char LastTime[25] = "0000-00-00T00:00:00.000Z";
volatile char LastTime[25] = "0000-00-00T00:00:00.000Z";
volatile char LastTime_hb[25] = "0000-00-00T00:00:00.000Z";

// cached TPM registration ID if TPM auth is used
// once (if) we support a discpose method, we should free this value
static char* tpm_registration_id = NULL;



static void dump_response(const char* message, IotConnectHttpResponse* response) {
    fprintf(stderr, "%s", message);
    if (response->data) {
        fprintf(stderr, " Response was:\n----\n%s\n----\n", response->data);
    }
    else {
        fprintf(stderr, " Response was empty\n");
    }
}

static void report_sync_error(IotclSyncResponse* response, const char* sync_response_str) {
    if (NULL == response) {
        fprintf(stderr, "Failed to obtain sync response?\n");
        return;
    }
    switch (response->ec) {
    case IOTCL_SR_DEVICE_NOT_REGISTERED:
        fprintf(stderr, "IOTC_SyncResponse error: Not registered\n");
        break;
    case IOTCL_SR_AUTO_REGISTER:
        fprintf(stderr, "IOTC_SyncResponse error: Auto Register\n");
        break;
    case IOTCL_SR_DEVICE_NOT_FOUND:
        fprintf(stderr, "IOTC_SyncResponse error: Device not found\n");
        break;
    case IOTCL_SR_DEVICE_INACTIVE:
        fprintf(stderr, "IOTC_SyncResponse error: Device inactive\n");
        break;
    case IOTCL_SR_DEVICE_MOVED:
        fprintf(stderr, "IOTC_SyncResponse error: Device moved\n");
        break;
    case IOTCL_SR_CPID_NOT_FOUND:
        fprintf(stderr, "IOTC_SyncResponse error: CPID not found\n");
        break;
    case IOTCL_SR_UNKNOWN_DEVICE_STATUS:
        fprintf(stderr, "IOTC_SyncResponse error: Unknown device status error from server\n");
        break;
    case IOTCL_SR_ALLOCATION_ERROR:
        fprintf(stderr, "IOTC_SyncResponse internal error: Allocation Error\n");
        break;
    case IOTCL_SR_PARSING_ERROR:
        fprintf(stderr,
            "IOTC_SyncResponse internal error: Parsing error. Please check parameters passed to the request.\n");
        break;
    default:
        fprintf(stderr, "WARN: report_sync_error called, but no error returned?\n");
        break;
    }
    fprintf(stderr, "Raw server response was:\n--------------\n%s\n--------------\n", sync_response_str);
}

////////////////////////////// Edit Discovery using SID ///////////////////////////////////

static IotclDiscoveryResponse* run_http_discovery_sid(const char* sid)
{
    IotclDiscoveryResponse* ret = NULL;
    char* url_buff = malloc(sizeof(HTTP_DISCOVERY_URL_FORMAT) +
        sizeof(IOTCONNECT_DISCOVERY_HOSTNAME) +
        strlen(sid) //+ re-edit
       // strlen(env) - 4 /* %s x 2 */ re-edit
    );

    /*sprintf(url_buff, HTTP_DISCOVERY_URL_FORMAT,
        IOTCONNECT_DISCOVERY_HOSTNAME, cpid, env
    );  re-edit */

    sprintf(url_buff, HTTP_DISCOVERY_URL_FORMAT,
        IOTCONNECT_DISCOVERY_HOSTNAME, sid
    );

    //printf("Discovery url %s \n", url_buff);
    
    

    IotConnectHttpResponse response;
    iotconnect_https_request(&response,
        url_buff,
        NULL
    );

    if (NULL == response.data) {
        dump_response("Unable to parse HTTP response,", &response);
        goto cleanup;
    }
    char* json_start = strstr(response.data, "{");
    if (NULL == json_start) {
        dump_response("No json response from server.", &response);
        goto cleanup;
    }
    if (json_start != response.data) {
        dump_response("WARN: Expected JSON to start immediately in the returned data.", &response);
    }

    ret = iotcl_discovery_parse_discovery_response(json_start);
    if (!ret) {
        // fprintf(stderr, "Error: Unable to get discovery response for environment \"%s\". Please check the environment name in the key vault.\n", env);//Re-edit
    }

cleanup:
    iotconnect_free_https_response(&response);
    // fall through
    return ret;
}

///////////////////////////////////Fuction Ritern ////////////////////////////////////////

static IotclSyncResponse* run_http_sync(const char* cpid, const char* uniqueid) {
    IotclSyncResponse* ret = NULL;
    /* char* url_buff = malloc(sizeof(HTTP_SYNC_URL_FORMAT) +
    *                          sizeof(IOTCONNECT_DISCOVERY_PROTOCOL_POST_DATA_MAX_LEN)+
                            strlen(discovery_response->host) +
                            strlen(discovery_response->path)
    );*/ //Re-Edit

    char* url_buff = malloc(sizeof(HTTP_SYNC_URL_FORMAT) +
        strlen(discovery_response->url) +
        strlen(uniqueid)
    );

    char* post_data = malloc(IOTCONNECT_DISCOVERY_PROTOCOL_POST_DATA_MAX_LEN + 1);

    if (!url_buff || !post_data) {
        fprintf(stderr, "run_http_sync: Out of memory!");
        free(url_buff); // one of them could have succeeded
        free(post_data);
        return NULL;
    }

    /*sprintf(url_buff, HTTP_SYNC_URL_FORMAT,
            discovery_response->host,
            discovery_response->path
    );*///Re-edit

    sprintf(url_buff, HTTP_SYNC_URL_FORMAT,
        discovery_response->url,
        uniqueid
    );//edit

    //printf("\nsync url %s\n", url_buff);

    snprintf(post_data,
        IOTCONNECT_DISCOVERY_PROTOCOL_POST_DATA_MAX_LEN, /*total length should not exceed MTU size*/
        IOTCONNECT_DISCOVERY_PROTOCOL_POST_DATA_TEMPLATE,
        cpid,
        uniqueid
    );

    IotConnectHttpResponse response;
    /*iotconnect_https_request(&response,
                             url_buff,
                             post_data
    );*/// Re-edit

    iotconnect_https_request(&response,
        url_buff,
        NULL
    );//edit

    free(url_buff);
    free(post_data);

    if (NULL == response.data) {
        dump_response("Unable to parse HTTP response.", &response);
        goto cleanup;
    }
    char* json_start = strstr(response.data, "{");
    if (NULL == json_start) {
        dump_response("No json response from server.", &response);
        goto cleanup;
    }
    if (json_start != response.data) {
        dump_response("WARN: Expected JSON to start immediately in the returned data.", &response);
    }

    ret = iotcl_discovery_parse_sync_response(json_start);
    if (!ret || ret->ec != IOTCL_SR_OK)
    {
        if (config.auth_info.type == IOTC_AT_TPM && ret && ret->ec == IOTCL_SR_DEVICE_NOT_REGISTERED)
        {
            // malloc below will be freed when we iotcl_discovery_free_sync_response
            ret->broker.client_id = malloc(strlen(uniqueid) + 1 /* - */ + strlen(cpid) + 1);
            sprintf(ret->broker.client_id, "%s-%s", cpid, uniqueid);
            printf("TPM Device is not yet enrolled. Enrolling...\n");
        }
        else
        {
            report_sync_error(ret, response.data);
            iotcl_discovery_free_sync_response(ret);
            ret = NULL;
        }
    }

cleanup:
    iotconnect_free_https_response(&response);
    // fall through

    return ret;
}




static void on_mqtt_c2d_message(unsigned char* message, size_t message_len)
{
    char* str = malloc(message_len + 1);
    memcpy(str, message, message_len);
    str[message_len] = 0;
    printf("event>>> %s\n\n", str);
    if (!iotcl_process_event(str)) {
        fprintf(stderr, "Error encountered while processing %s\n", str);
    }
    free(str);
}

static void on_twin_c2d_message(unsigned char* message, size_t message_len)//edit twin
{
    char* str = malloc(message_len + 1);
    memcpy(str, message, message_len);
    str[message_len] = 0;
    printf("event>>> %s\n", str);


    if (!twin_event(str))
    {
        fprintf(stderr, "Error encountered while processing %s\n", str);
    }

    free(str);
}

void Dispose() {
    printf("Disconnecting...\n");
    if (0 == iotc_device_client_disconnect()) {
        printf("Disconnected.\n");
    }
}


void iotconnect_sdk_disconnect() {
    printf("Disconnecting...\n");
    if (0 == iotc_device_client_disconnect()) {
        printf("Disconnected.\n");
    }
}

bool iotconnect_sdk_is_connected() {
    return iotc_device_client_is_connected();
}

IotConnectClientConfig* iotconnect_sdk_init_and_get_config() {
    memset(&config, 0, sizeof(config));
    return &config;
}

IotclConfig* iotconnect_sdk_get_lib_config() {
    return iotcl_get_config();
}

/*
 void getattributes(IotclEventData command)
{
   const char* get_att_str = prosess_cmd(command);


   printf("Send get attribute cmd %s\n", get_att_str);
   iotconnect_sdk_send_di_packet(get_att_str);
   free((void*)get_att_str);

} */
 
/*
 void updateTwin(IotclEventData key, IotclEventData value)//edit twin
{
    const char* twin_json_report = create_twin_json(key, value);

    printf("Twin Json to Report %s\n", twin_json_report);
    iotconnect_sdk_send_twin_packet(twin_json_report);
    free((void*)twin_json_report);

}*/

void updateTwin(const char* key, const char* value)//edit twin
{
    const char* twin_json_report = create_twin_json(key, value);

    printf("\nTwin Json to Report %s\n", twin_json_report);
    if (!twin_json_report)
    {
        free((void*)twin_json_report);
    }
    else
    {
        iotconnect_sdk_send_twin_packet(twin_json_report);
        free((void*)twin_json_report);
    }
    

}

void updateTwin_bool(const char* key, bool value)
{
    char* result = NULL;
    const char* twin_json_report = NULL;
    cJSON* twin_json = cJSON_CreateObject();
    if (!twin_json)
    {
        return NULL;
    }
    if (!cJSON_AddBoolToObject(twin_json, key, value)) goto cleanup;
    result = cJSON_PrintUnformatted(twin_json);
    printf("\nTwin Json to Report %s\n", result);
    iotconnect_sdk_send_twin_packet(result);
    free((void*)result);

cleanup:
    cJSON_Delete(twin_json);
    //return result;

}


void updateTwin_string(const char* key, const char *value)
{
    char* result = NULL;
    const char* twin_json_report = NULL;
    cJSON* twin_json = cJSON_CreateObject();
    if (!twin_json)
    {
        return NULL;
    }
    if (!cJSON_AddStringToObject(twin_json, key, value)) goto cleanup;
    result = cJSON_PrintUnformatted(twin_json);
    printf("\nTwin Json to Report %s\n", result);
    iotconnect_sdk_send_twin_packet(result);
    free((void*)result);

cleanup:
    cJSON_Delete(twin_json);
    //return result;

}

void updateTwin_int(const char* key, int value)
{
    char* result = NULL;
    const char* twin_json_report = NULL;
    cJSON* twin_json = cJSON_CreateObject();
    if (!twin_json)
    {
        return NULL;
    }
    if (!cJSON_AddNumberToObject(twin_json, key, value)) goto cleanup;
    result = cJSON_PrintUnformatted(twin_json);
    printf("\nTwin Json to Report %s\n", result);
    iotconnect_sdk_send_twin_packet(result);
    free((void*)result);

cleanup:
    cJSON_Delete(twin_json);
    //return result;

}

void updateTwin_float(const char* key, double value)
{
    char* result = NULL;
    const char* twin_json_report = NULL;
    cJSON* twin_json = cJSON_CreateObject();
    if (!twin_json)
    {
        return NULL;
    }
    if (!cJSON_AddNumberToObject(twin_json, key, value)) goto cleanup;
    result = cJSON_PrintUnformatted(twin_json);
    printf("\nTwin Json to Report %s\n", result);
    iotconnect_sdk_send_twin_packet(result);
    free((void*)result);

cleanup:
    cJSON_Delete(twin_json);
    //return result;
}

void updateTwin_test(const char* key, void* value, int type)//edit twin
{

    char* result = NULL;
    const char* twin_json_report = NULL;
    cJSON* twin_json = cJSON_CreateObject();

    if (!twin_json)
    {
        return NULL;
    }
    if (type == 2)
    {
        //bool twin_value = *(int*)value;

        
        if (!cJSON_AddNumberToObject(twin_json, key, *(int*)value)) goto cleanup;
        twin_json_report = cJSON_PrintUnformatted(twin_json);
    }
    if (type == 16)
    {
        twin_t* localTwinPtr = (twin_t*)value;
        (char*)localTwinPtr->twinVar;
        
        //strcpy((char*)localTwinPtr->twinVar, (*char)*value);


    }
    
    //const char* twin_json_report = create_twin_json(key, value);

    printf("\nTwin Json to Report %s\n", twin_json_report);
    if (!twin_json_report)
    {
        free((void*)twin_json_report);
    }
    else
    {
        iotconnect_sdk_send_twin_packet(twin_json_report);
        free((void*)twin_json_report);
    }
cleanup:
    cJSON_Delete(twin_json);
   // return result;

}


void deleteChildDevice(char* deviceId) 
{
    if (sync_d_pro() == 1)
    {
        int command = 222;
        char* result = NULL;

        cJSON* data;
        cJSON* child_root = cJSON_CreateObject();
        if (!child_root)
        {
            return NULL;
        }

        cJSON_AddNumberToObject(child_root, "mt", command);
        cJSON_AddItemToObject(child_root, "d", data = cJSON_CreateObject());
        cJSON_AddStringToObject(data, "id", deviceId);
        result = cJSON_PrintUnformatted(child_root);
        printf("Creat Child Json %s \n", result);

        iotconnect_sdk_send_di_packet(result);
    }
    else
    {
        printf("Device is not Gatway Device\n");
    }

}


void createChildDevice(char* deviceId, char* deviceTag, char* displayName)
{
    if (sync_d_pro() == 1)
    {

        int command = 221;
        char* result = NULL;

        cJSON* data;
        cJSON* child_root = cJSON_CreateObject();
        if (!child_root)
        {
            return NULL;
        }

        cJSON_AddNumberToObject(child_root, "mt", command);
        cJSON_AddItemToObject(child_root, "d", data = cJSON_CreateObject());
        cJSON_AddStringToObject(data, "g", sync_response->meta.g);
        cJSON_AddStringToObject(data, "dn", displayName);
        cJSON_AddStringToObject(data, "id", deviceId);
        cJSON_AddStringToObject(data, "tg", deviceTag);


        result = cJSON_PrintUnformatted(child_root);
        printf("Creat Child Json %s \n", result);

        iotconnect_sdk_send_di_packet(result);
        //const char* creatchild = prosess_cmd(command);
    }
    else
    {
        printf("Device is not Gatway Device\n");
    }

}


char* iotcl_process_twin_event(const char* event, const char* twin_user_key)
{
   
        //const char* Twin_key;
        const char* Twin_value;
        IotclConfig* config = iotcl_get_config();
        if (!config)
        {
            return NULL;
        }

        printf("Twin recieve: %s \n", event);
        cJSON* rootProperties = cJSON_Parse(event);
        if (!rootProperties)
        {
            printf("WARNING: Cannot parse the string as JSON content.\n");
            return NULL;
        }

        cJSON* desiredProperties = cJSON_GetObjectItem(rootProperties, "desired");
        if (!desiredProperties)
        {
            desiredProperties = rootProperties;
        }

        //cJSON* key = cJSON_GetObjectItemCaseSensitive(desiredProperties, "StatusLED");twin_user_key
        cJSON* key = cJSON_GetObjectItemCaseSensitive(desiredProperties, twin_user_key);
        if (key)
        {
            //Twin_key = twin_user_key;
            Twin_value = key->valuestring;
            return Twin_value;
            //config->event_functions.twin_up_cb(Twin_key, Twin_value);
        }
        else
        {
            printf("twin key is not matching %s\n", twin_user_key);
             return NULL;
        }
    cleanup:
        cJSON_Delete(desiredProperties);

}
static void stop_heartbeat(IotclEventData data)
{
    sync_response->meta.hb_event = hb_event(data);
}

static void start_heartbeat(IotclEventData data)
{
    sync_response->meta.start_hb = hb_update(data);//'f' = 5
    sync_response->meta.hb_event = hb_event(data);
    //prosess_cmd
    //char* hb_snd = prosess_hb();
    //iotconnect_sdk_send_hb_packet(hb_snd);
}



static void get_df(IotclEventData data)
{
    sync_response->meta.df = df_update(data);

}


static void device_Resp(IotclEventData data)
{
    //printf("In Device_Resp_function().........\n");

        char* in_data; 
        char* json_data;
        int xx = 0;
        cJSON* in_CMD;
        cJSON* has; 
        cJSON* value;
        cJSON* obj;
        cJSON* meta;
        //in_CMD = cJSON_Parse(data);
        json_data = id_tg(data);
        in_CMD = cJSON_Parse(json_data);
        
        /*
        {
          "d": {
                "d": [{
                        "tg": "ch1",
                        "id": "windemoch1"
                        }],
                "ct": 204,
                "ec": 0,
                "dt": "2022-02-07T19:27:52.6902168Z"
                }
        }
        */
        in_data = cJSON_GetObjectItem(in_CMD, "d");
        if (cJSON_IsObject(in_data)) 
        {
            has = cJSON_GetObjectItem(in_data, "d");
            if (cJSON_IsArray(has)) 
            {
                for (xx = 0; xx < cJSON_GetArraySize(has); xx++) 
                {
                    obj = cJSON_GetArrayItem(has, xx);
                    value = cJSON_GetObjectItem(obj, "id");
                    SYNC_resp.dvc_ID[xx] = value->valuestring;
                    value = cJSON_GetObjectItem(obj, "tg");
                    SYNC_resp.tag[xx] = value->valuestring;
                    //printf(" child ID[%d] -> %s\n tag tg[%d] -> %s\n", xx, SYNC_resp.dvc_ID[xx], xx, SYNC_resp.tag[xx]);
                }
                
            }
        }
        else
            return;
}


static void ondevicechangecommand(IotclEventData data, IotConnectEventType type)
{

    switch (type) {
    case ON_FORCE_SYNC:
        iotconnect_sdk_disconnect();
        iotcl_discovery_free_discovery_response(discovery_response);
        iotcl_discovery_free_sync_response(sync_response);
        sync_response = NULL;
        discovery_response = run_http_discovery_sid(config.sid);
        if (NULL == discovery_response) {
            fprintf(stderr, "Unable to run HTTP discovery on ON_FORCE_SYNC\n");
            return;
        }
        sync_response = run_http_sync(config.cpid, config.duid);
        if (NULL == sync_response) {
            fprintf(stderr, "Unable to run HTTP sync on ON_FORCE_SYNC\n");
            return;
        }
        printf("Got ON_FORCE_SYNC. Disconnecting.\n");
        iotconnect_sdk_disconnect(); // client will get notification that we disconnected and will reinit
        break;

    case ON_CLOSE:
        printf("Got a disconnect request. Closing the mqtt connection. Device restart is required.\n");
        iotconnect_sdk_disconnect();
        break;

    case DEVICE_DELETE:
        printf("Got a disconnect request. Closing the mqtt connection. Device restart is required.\n");
        iotconnect_sdk_disconnect();
        break;

    case DEVICE_DISABLE:
        printf("Got a disconnect request. Closing the mqtt connection. Device restart is required.\n");
        iotconnect_sdk_disconnect();
        break;

    case STOP_OPERATION:
        printf("Got a disconnect request. Closing the mqtt connection. Device restart is required.\n");
        iotconnect_sdk_disconnect();
        break;

    default:
        break; // not handling nay other messages
    }

    if (NULL != config.msg_cb) {
        config.msg_cb(data, type);
    }
}

int iotconnect_sdk_send_hb_packet(const char* data)
{
    int send_tel = NULL;
    char* NowTime = iotcl_iso_timestamp_now();
    long int Timediff = GetTimeDiff(NowTime, LastTime);
    if (sync_response->meta.start_hb <= Timediff)
    {
        send_tel = iotc_device_client_send_hb_message(data);
        printf("Sending Start Heart Beat: %s\n", data);

        for (int ss = 0; ss < 25; ss++)
            LastTime[ss] = NowTime[ss];
    }
    //printf("time diffrence %d\n", Timediff);
    return send_tel;
}

char* sync_tg() {

    return sync_response->meta.tg;
}

int sync_d_pro()
{
    return sync_response->has.d;

}

int iotconnect_sdk_send_packet_test(const char* data)
{
    int send_tel = NULL;
    char* NowTime = iotcl_iso_timestamp_now();
    long int Timediff = GetTimeDiff(NowTime, LastTime);
    if (sync_response->meta.df <= Timediff)
    {
        int x = 0, y = 0;
        char* in_time[10], * in_ID[10], * in_data[10];

        cJSON* value;
        cJSON* in_obj;
        cJSON* TO_HUB;
        cJSON* sdk;
        cJSON* device;
        cJSON* Arraydevice;
        cJSON* Arraydata;

        cJSON* in_att = cJSON_Parse(data);
        if (in_att == NULL) 
        {
            const char* error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) 
            {
                printf(stderr, "Error before: %s\n", error_ptr);
            }
        }

        int Device = cJSON_GetArraySize(in_att);
        for (x = 0; x < Device; x++)
        {
            in_obj = cJSON_GetArrayItem(in_att, x);
            value = cJSON_GetObjectItem(in_obj, "uniqueId");
            if (cJSON_IsString(value))
            {
                in_ID[x] = value->valuestring;
            }   
            else 
            {
                printf("\r\n\t ERR_SD08   Missing required parameter 'uniqueId' \r\n");
            }

            value = cJSON_GetObjectItem(in_obj, "time");
            if (cJSON_IsString(value))
            {
                in_time[x] = value->valuestring;
            }
            else
            {
                printf("\r\n\t ERR_SD07   Missing required parameter 'time' \r\n");
            }

            in_data[x] = cJSON_GetObjectItem(in_obj, "data");
            if (cJSON_IsObject(in_data[x]));
            else printf("\r\n\t ERR_SD06   Missing required parameter 'data' \r\n");
        }

        in_time[0] = NowTime;
        TO_HUB = cJSON_CreateObject();
        //cJSON_AddStringToObject(TO_HUB, "sid", "your_SID");
        //cJSON_AddNumberToObject(TO_HUB, "mt", 0);
        cJSON_AddStringToObject(TO_HUB, "dt", in_time[0]);
        //cJSON_AddStringToObject(TO_HUB, "dtg", "DTG");
        cJSON_AddItemToObject(TO_HUB, "d", Arraydevice = cJSON_CreateArray());
        for (x = 0; x < Device; x++)
        {
            cJSON_AddItemToArray(Arraydevice, device = cJSON_CreateObject());
            cJSON_AddStringToObject(device, "id", in_ID[x]);
            for (y = 0; y < 10; y++)
            {
                
                if (SYNC_resp.dvc_ID[y] != '\0')
                {
                    if (strcmp(in_ID[x], SYNC_resp.dvc_ID[y]) == 0)
                       
                        break;
                    else
                        continue;
                }
            }

            if (sync_response->has.d == 1 && !(strcmp(in_ID[x], config.duid) == 0))
                cJSON_AddStringToObject(device, "tg", SYNC_resp.tag[y] );
            else
                cJSON_AddStringToObject(device, "tg", sync_tg());

            cJSON_AddStringToObject(device, "dt", in_time[0]);
            cJSON_AddItemToObject(device, "d", in_data[x]);
        }
        char* TO_HUB_data = cJSON_Print(TO_HUB);
        send_tel = iotc_device_client_send_message(TO_HUB_data);
        printf("Sending: %s\n", TO_HUB_data);

        for (int ss = 0; ss < 25; ss++)
            LastTime[ss] = NowTime[ss];
    }
    
    if (sync_response->meta.hb_event == 110)
    {
        char* NowTime_hb = iotcl_iso_timestamp_now();
        long int Timediff_hb = GetTimeDiff_hb(NowTime_hb, LastTime_hb);

        if (sync_response->meta.start_hb <= Timediff_hb)
        {
            printf("Sending: Heart Beat %s\n", NowTime_hb);
            char* hb_snd = prosess_hb();
            iotc_device_client_send_hb_message(hb_snd);
            
            for (int ss = 0; ss < 25; ss++)
                LastTime_hb[ss] = NowTime_hb[ss];
        }
    }
    
    //printf("time diffrence %d\n", Timediff);
    return send_tel;
 }

 int GetTimeDiff_hb(char newT[25], char oldT[25]) {
     time_t NEW, OLD;
     struct tm new_date;
     struct tm old_date;
     unsigned int DHour, DMin, DSec;

     new_date.tm_mday = ((newT[8] - '0') * 10 + (newT[9] - '0'));    new_date.tm_mon = ((newT[5] - '0') * 10 + (newT[6] - '0'));
     new_date.tm_year = ((newT[3] - '0') * 1000 + (newT[2] - '0') * 100 + (newT[1] - '0') * 10 + (newT[0] - '0'));
     new_date.tm_hour = ((newT[11] - '0') * 10 + (newT[12] - '0'));  new_date.tm_min = ((newT[14] - '0') * 10 + (newT[15] - '0'));
     new_date.tm_sec = ((newT[17] - '0') * 10 + (newT[18] - '0'));

     old_date.tm_mday = ((oldT[8] - '0') * 10 + (oldT[9] - '0'));    old_date.tm_mon = ((oldT[5] - '0') * 10 + (oldT[6] - '0'));
     old_date.tm_year = ((oldT[3] - '0') * 1000 + (oldT[2] - '0') * 100 + (oldT[1] - '0') * 10 + (oldT[0] - '0'));
     old_date.tm_hour = ((oldT[11] - '0') * 10 + (oldT[12] - '0'));  old_date.tm_min = ((oldT[14] - '0') * 10 + (oldT[15] - '0'));
     old_date.tm_sec = ((oldT[17] - '0') * 10 + (oldT[18] - '0'));

     NEW = mktime(&new_date);
     OLD = mktime(&old_date);
     while (old_date.tm_sec > new_date.tm_sec) {
         --new_date.tm_min;
         new_date.tm_sec += 60;
     }

     DSec = new_date.tm_sec - old_date.tm_sec;
     while (old_date.tm_min > new_date.tm_min) {
         --new_date.tm_hour;
         new_date.tm_min += 60;
     }
     DMin = new_date.tm_min - old_date.tm_min;
     DHour = new_date.tm_hour - old_date.tm_hour;

     unsigned int TIMEDIFF_HB = (DHour * 60 * 60) + (DMin * 60) + (DSec);
     return TIMEDIFF_HB;
 }



int iotconnect_sdk_send_packet(const char* data)
{
    int send_tel = NULL;
    char* NowTime = iotcl_iso_timestamp_now();
    long int Timediff = GetTimeDiff(NowTime, LastTime);
    if (sync_response->meta.df <= Timediff)
    {
        send_tel = iotc_device_client_send_message(data);
        printf("Sending: %s\n", data);

        for (int ss = 0; ss < 25; ss++)
            LastTime[ss] = NowTime[ss];
    }
    printf("time diffrence %d\n", Timediff);
    return send_tel;
}



int GetTimeDiff(char newT[25], char oldT[25]) {
    time_t NEW, OLD;
    struct tm new_date;
    struct tm old_date;
    unsigned int DHour, DMin, DSec;

    new_date.tm_mday = ((newT[8] - '0') * 10 + (newT[9] - '0'));    new_date.tm_mon = ((newT[5] - '0') * 10 + (newT[6] - '0'));
    new_date.tm_year = ((newT[3] - '0') * 1000 + (newT[2] - '0') * 100 + (newT[1] - '0') * 10 + (newT[0] - '0'));
    new_date.tm_hour = ((newT[11] - '0') * 10 + (newT[12] - '0'));  new_date.tm_min = ((newT[14] - '0') * 10 + (newT[15] - '0'));
    new_date.tm_sec = ((newT[17] - '0') * 10 + (newT[18] - '0'));

    old_date.tm_mday = ((oldT[8] - '0') * 10 + (oldT[9] - '0'));    old_date.tm_mon = ((oldT[5] - '0') * 10 + (oldT[6] - '0'));
    old_date.tm_year = ((oldT[3] - '0') * 1000 + (oldT[2] - '0') * 100 + (oldT[1] - '0') * 10 + (oldT[0] - '0'));
    old_date.tm_hour = ((oldT[11] - '0') * 10 + (oldT[12] - '0'));  old_date.tm_min = ((oldT[14] - '0') * 10 + (oldT[15] - '0'));
    old_date.tm_sec = ((oldT[17] - '0') * 10 + (oldT[18] - '0'));

    NEW = mktime(&new_date);
    OLD = mktime(&old_date);
    while (old_date.tm_sec > new_date.tm_sec) {
        --new_date.tm_min;
        new_date.tm_sec += 60;
    }

    DSec = new_date.tm_sec - old_date.tm_sec;
    while (old_date.tm_min > new_date.tm_min) {
        --new_date.tm_hour;
        new_date.tm_min += 60;
    }
    DMin = new_date.tm_min - old_date.tm_min;
    DHour = new_date.tm_hour - old_date.tm_hour;

    unsigned int TIMEDIFF = (DHour * 60 * 60) + (DMin * 60) + (DSec);
    return TIMEDIFF;
}


int iotconnect_sdk_send_ack_packet(const char* data) {
    return iotc_device_client_send_ack_message(data);
}

int iotconnect_sdk_send_twin_packet(const char* data) {//edit twin
    return iotc_device_client_send_twin_message(data);
}

int iotconnect_sdk_send_di_packet(const char* data) {//edit Divice Identity
    return iotc_device_client_send_di_message(data);
}


void iotconnect_sdk_receive() {
    return iotc_device_client_receive();
}

///////////////////////////////////////////////////////////////////////////////////
// this the Initialization os IoTConnect SDK
int iotconnect_sdk_init() {
    int ret;

    if (config.auth_info.type == IOTC_AT_TPM) {
        if (!config.duid || strlen(config.duid) == 0) {
            if (!tpm_registration_id) {
                tpm_registration_id = iotc_device_client_get_tpm_registration_id();
            }
            config.duid = tpm_registration_id;
        }
    }

    if (!discovery_response) {
        discovery_response = run_http_discovery_sid(config.sid);
        if (NULL == discovery_response) {
            // get_base_url will print the error
            return -1;
        }
        printf("Discovery response parsing successful.\n");
    }

    if (!sync_response) {
        sync_response = run_http_sync(config.cpid, config.duid);
        if (NULL == sync_response) {
            // Sync_call will print the error
            return -2;
        }
        printf("Sync response parsing successful.\n");
    }

    // We want to print only first 4 characters of cpid
    lib_config.device.env = config.env;
    lib_config.device.cpid = config.cpid;
    lib_config.device.duid = config.duid;

    if (!config.env || !config.cpid || !config.duid) {
        printf("Error: Device configuration is invalid. Configuration values for env, cpid and duid are required.\n");
        return -1;
    }

    lib_config.event_functions.ota_cb = config.ota_cb;
    lib_config.event_functions.mod_cb = config.mod_cb;
    lib_config.event_functions.cmd_cb = config.cmd_cb;
    lib_config.event_functions.twin_msg_rciv = config.twin_msg_rciv;
    lib_config.event_functions.getatt_cb = config.getatt_cb;
    lib_config.event_functions.get_ch = config.get_ch;
    lib_config.event_functions.rule_cb = config.rule_cb;
    lib_config.event_functions.gettwin_cb = config.gettwin_cb;

    lib_config.event_functions.resp_recive = config.resp_recive;
    lib_config.event_functions.msg_cb = ondevicechangecommand;
    lib_config.event_functions.child_dev = device_Resp;
    lib_config.event_functions.twin_up_cb = updateTwin;
    lib_config.event_functions.get_df = get_df;
    lib_config.event_functions.hb_cmd = start_heartbeat;
    lib_config.event_functions.hb_stop = stop_heartbeat;
    lib_config.telemetry.dtg = sync_response->dtg;

    char cpid_buff[5];
    strncpy(cpid_buff, config.cpid, 4);
    cpid_buff[4] = 0;
    printf("CPID: %s***\n", cpid_buff);
    printf("ENV:  %s\n", config.env);


    if (config.auth_info.type != IOTC_AT_TOKEN &&
        config.auth_info.type != IOTC_AT_X509 &&
        config.auth_info.type != IOTC_AT_TPM &&
        config.auth_info.type != IOTC_AT_SYMMETRIC_KEY
        ) {
        fprintf(stderr, "Error: Unsupported authentication type!\n");
        return -1;
    }

    if (!config.auth_info.trust_store) {
        fprintf(stderr, "Error: Configuration server certificate is required.\n");
        return -1;
    }
    if (config.auth_info.type == IOTC_AT_X509 && (
        !config.auth_info.data.cert_info.device_cert ||
        !config.auth_info.data.cert_info.device_key)) {
        fprintf(stderr, "Error: Configuration authentication info is invalid.\n");
        return -1;
    }

    if (config.auth_info.type == IOTC_AT_SYMMETRIC_KEY) {
        if (config.auth_info.data.symmetric_key && strlen(config.auth_info.data.symmetric_key) > 0) {
#ifdef IOTC_USE_PAHO
            // for paho we need to pass the generated sas token
            char* sas_token = malloc(128 + 1);

            sas_token = gen_sas_token(sync_response->broker.host,
                config.cpid,
                config.duid,
                config.auth_info.data.symmetric_key,
                60
            );
            free(sync_response->broker.pass);
            // a bit of a hack - the token will be freed when freeing the sync response
            // paho will use the borken pasword
            sync_response->broker.pass = sas_token;

            //free(sas_token);

#endif
        }
        else {
            fprintf(stderr, "Error: Configuration symmetric key is missing.\n");
            return -1;
        }
    }

    if (config.auth_info.type == IOTC_AT_TOKEN) {
        if (!(sync_response->broker.pass || strlen(config.auth_info.data.symmetric_key) == 0)) {
            fprintf(stderr, "Error: Unable to obtainm .\n");
            return -1;
        }
    }
    if (!iotcl_init(&lib_config)) {
        fprintf(stderr, "Error: Failed to initialize the IoTConnect Lib\n");
        return -1;
    }

    IotConnectDeviceClientConfig pc;
    pc.sr = sync_response;
    pc.qos = config.qos;
    pc.status_cb = config.status_cb;
    pc.c2d_msg_cb = on_mqtt_c2d_message;
    //pc.child_dev = device_Resp;
    pc.c2d_twin_cb = on_twin_c2d_message;//edit twin
    pc.auth = &config.auth_info;

    ret = iotc_device_client_init(&pc);
    if (ret) {
        fprintf(stderr, "Failed to connect!\n");
        return ret;
    }

    if (sync_d_pro() == 1) {
        int ch_cmd = 204;
        config.get_ch(ch_cmd);
    }
    for (int k = 0; k < 1000; k++) {
        iotconnect_sdk_receive();
        _sleep(10);
    }

     if (sync_response->has.attr == 1)
    {
        int attr_cmd = 201;
        
        config.getatt_cb(attr_cmd);
    }

    for (int k = 0; k < 1000; k++) {
        iotconnect_sdk_receive();
        _sleep(10);
    }

    if (sync_response->has.set == 1)
    {
        int twin_cmd = 202;

        config.gettwin_cb(twin_cmd);
    }

    for (int k = 0; k < 1000; k++) {
        iotconnect_sdk_receive();
        _sleep(10);
    }




    // Workaround: upon first time TPM registration, the information returned from sync will be partial,
    // so update dtg with new sync call
    if (config.auth_info.type == IOTC_AT_TPM && sync_response->ec == IOTCL_SR_DEVICE_NOT_REGISTERED) {
        iotcl_discovery_free_sync_response(sync_response);
        sync_response = run_http_sync(config.cpid, config.duid);
        lib_config.telemetry.dtg = sync_response->dtg;
        if (NULL == sync_response) {
            // Sync_call will print the error
            return -2;
        }
        printf("Secondary Sync response parsing successful. DTG is: %s.\n", sync_response->dtg);
    }

    return ret;
}
