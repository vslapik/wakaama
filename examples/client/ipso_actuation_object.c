/*
 * Implements an object for actuation device
 * This object model is based on IPSO Smart Object design Pattern
 * See this document for more information about IPSO Smart Object: http://www.ipso-alliance.org/wp-content/uploads/2016/01/ipso-paper.pdf
 * To get the IPSO Starter Pack visit this page : http://www.ipso-alliance.org/so-starter-pack/
 *
 *                              Multiple
 *       Object       |  ID  | Instances | Mandatory |                          Description                        |
 *  IPSO Actuation    | 3306 |    Yes    |    No     | Actuator object with on/off control and proportional control|
 *
 *  Resources:
 *              Supported    Multiple
 *  Name    | ID | Operations | Instances | Mandatory |  Type   | Range | Units | Description       |
 *  On/Off  |5850|    R/W     |    No     | Yes       | Boolean |       |       | On/off control,   |
 *          |    |            |           |           |         |       |       | 0=OFF, 1=ON       |
 *  Dimmer  |5851|    R/W     |    No     | Optional  | Integer | 0-100 |   %   | Proportional      |
 *          |    |            |           |           |         |       |       | control, integer  |
 *          |    |            |           |           |         |       |       | value between 0   |
 *          |    |            |           |           |         |       |       | and 100 as a      |
 *          |    |            |           |           |         |       |       | percentage.       |
 *  On Time |5852|    R/W     |    No     | Optional  | Integer |       |   s   | The time in       |
 *          |    |            |           |           |         |       |       | seconds that the  |
 *          |    |            |           |           |         |       |       | device has been   |
 *          |    |            |           |           |         |       |       | on. Writing a     |
 *          |    |            |           |           |         |       |       | value of 0 resets |
 *          |    |            |           |           |         |       |       | the counter.      |
 *  Multi   |5853|    R/W     |    No     | Optional  | String  |       |       | A string          |
 *  State   |    |            |           |           |         |       |       | describing a state|
 *  Output  |    |            |           |           |         |       |       | for multiple level|
 *          |    |            |           |           |         |       |       | output such as    |
 *          |    |            |           |           |         |       |       | Pilot Wire        |
 *  App.    |5750|    R/W     |    No     | Optional  | String  |       |       | The application   |
 *  Type    |    |            |           |           |         |       |       | type of the sensor|
 *          |    |            |           |           |         |       |       | or actuator as a  |
 *          |    |            |           |           |         |       |       | string, for       |
 *          |    |            |           |           |         |       |       | instance, “Air    |
 *          |    |            |           |           |         |       |       | Pressure”         |
 *
 */

#include "lwm2mclient.h"

#ifdef ESP8266
#include "periphery.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#define PROP_STR_BUF_LENGTH 64

#define ON_OFF_ID 5850
#define DIMMER_ID 5851
#define ON_TIME_ID 5852
#define MULTI_STATE_OUT_ID 5853
#define APP_TYPE_ID 5750

static lwm2m_context_t **lwm2mContext;        // pointer to the lwm2m context object

/*
 * Multiple instance objects can use userdata to store data that will be shared between the different instances.
 * The lwm2m_object_t object structure - which represent every object of the liblwm2m as seen in the single instance
 * object - contain a chained list called instanceList with the object specific structure _ipso_actuation_instance_t:
 */
typedef struct
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _prv_instance_ *next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    bool on_off;
    int64_t dimmer;
    int64_t on_time;
    char multi_state_out[PROP_STR_BUF_LENGTH];
    char app_type[PROP_STR_BUF_LENGTH];
} _ipso_actuation_instance_t;

static uint8_t prv_read(uint16_t instanceId,
                        int *numDataP,
                        lwm2m_data_t **dataArrayP,
                        lwm2m_object_t *objectP)
{
    _ipso_actuation_instance_t *targetP;

    targetP = (_ipso_actuation_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(5);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 5;
        (*dataArrayP)[0].id = ON_OFF_ID;
        (*dataArrayP)[1].id = DIMMER_ID;
        (*dataArrayP)[2].id = ON_TIME_ID;
        (*dataArrayP)[3].id = MULTI_STATE_OUT_ID;
        (*dataArrayP)[4].id = APP_TYPE_ID;
    }

    for (int i = 0 ; i < *numDataP ; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
        case ON_OFF_ID:
            lwm2m_data_encode_bool(targetP->on_off, *dataArrayP + i);
            break;
        case DIMMER_ID:
            lwm2m_data_encode_int(targetP->dimmer, *dataArrayP + i);
            break;
        case ON_TIME_ID:
            lwm2m_data_encode_int(targetP->on_time, *dataArrayP + i);
            break;
        case MULTI_STATE_OUT_ID:
            lwm2m_data_encode_string(targetP->multi_state_out, *dataArrayP + i);
            break;
        case APP_TYPE_ID:
            lwm2m_data_encode_string(targetP->app_type, *dataArrayP + i);
            break;
        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_205_CONTENT;
}

static uint8_t prv_discover(uint16_t instanceId,
                                   int *numDataP,
                                   lwm2m_data_t **dataArrayP,
                                   lwm2m_object_t *objectP)
{
    (void)objectP;
    uint8_t result;
    int i;

    // this is a single instance object
    if (instanceId != 0)
    {
        return COAP_404_NOT_FOUND;
    }

    result = COAP_205_CONTENT;

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        const uint16_t resList[] = {
            ON_OFF_ID,
            DIMMER_ID,
            ON_TIME_ID,
            MULTI_STATE_OUT_ID,
            APP_TYPE_ID
        };
        int nbRes = sizeof(resList) / sizeof(resList[0]);

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = nbRes;
        for (i = 0; i < nbRes; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }
    else
    {
        for (i = 0; i < *numDataP && result == COAP_205_CONTENT; i++)
        {
            switch ((*dataArrayP)[i].id)
            {
            case ON_OFF_ID:
            case DIMMER_ID:
            case ON_TIME_ID:
            case MULTI_STATE_OUT_ID:
            case APP_TYPE_ID:
                break;
            default:
                result = COAP_404_NOT_FOUND;
            }
        }
    }

    return result;
}

static uint8_t prv_write(uint16_t instanceId,
                         int numData,
                         lwm2m_data_t *dataArray,
                         lwm2m_object_t *objectP)
{
    _ipso_actuation_instance_t *targetP;

    targetP = (_ipso_actuation_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    for (int i = 0 ; i < numData ; i++)
    {
        switch (dataArray[i].id)
        {
        case ON_OFF_ID:
            if (1 != lwm2m_data_decode_bool(dataArray + i, &(targetP->on_off)))
            {   
                return COAP_400_BAD_REQUEST;
            }
            else {
                set_LED_state(targetP->on_off);
            }
            break;
        case DIMMER_ID:
            if (1 != lwm2m_data_decode_int(dataArray + i, &(targetP->dimmer)))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        case ON_TIME_ID:
            if (1 != lwm2m_data_decode_int(dataArray + i, &(targetP->on_time)))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        case MULTI_STATE_OUT_ID:
            if (NULL == strncpy(targetP->multi_state_out, (char *)(dataArray + i)->value.asBuffer.buffer, (dataArray + i)->value.asBuffer.length))
            {
                return COAP_400_BAD_REQUEST;
            }
            
            break;
        case APP_TYPE_ID:
            if (NULL == strncpy(targetP->app_type, (char *)(dataArray + i)->value.asBuffer.buffer, (dataArray + i)->value.asBuffer.length))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        default:
            return COAP_405_METHOD_NOT_ALLOWED;
        }
    }

    return COAP_204_CHANGED;
}

static uint8_t prv_delete(uint16_t id,
                          lwm2m_object_t *objectP)
{
    _ipso_actuation_instance_t *targetP;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&targetP);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    lwm2m_free(targetP);

    return COAP_202_DELETED;
}

static uint8_t prv_create(uint16_t instanceId,
                          int numData,
                          lwm2m_data_t *dataArray,
                          lwm2m_object_t *objectP)
{
    _ipso_actuation_instance_t *targetP;
    uint8_t result;


    targetP = lwm2m_malloc(sizeof(*targetP));
    if (NULL == targetP) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(targetP, 0, sizeof(*targetP));

    targetP->shortID = instanceId;
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);

    result = prv_write(instanceId, numData, dataArray, objectP);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_delete(instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}

static uint8_t prv_exec(uint16_t instanceId,
                        uint16_t resourceId,
                        uint8_t *buffer,
                        int length,
                        lwm2m_object_t *objectP)
{
    (void)instanceId;
    (void)resourceId;
    (void)buffer;
    (void)length;
    (void)objectP;

    return COAP_405_METHOD_NOT_ALLOWED;
}

void display_ipso_actuation_object(lwm2m_object_t *object)
{
#ifdef WITH_LOGS
    fprintf(stdout, "  /%u: Actuation object, instances:\r\n", object->objID);
    _ipso_actuation_instance_t *instance = (_ipso_actuation_instance_t *)object->instanceList;

    while (instance != NULL)
    {
        fprintf(stdout, "    /%u/%u: shortId: %u, on_off: %s, dimmer: %" PRId64 
                ", on_time: %" PRId64 ", multi_state_out: %s, app_type: %s\n",
                object->objID, instance->shortID,
                instance->shortID, instance->on_off ? "true" : "false",
                instance->dimmer, instance->on_time, 
                instance->multi_state_out, instance->app_type);

        instance = (_ipso_actuation_instance_t *)instance->next;
    }
#endif
}

lwm2m_object_t *get_ipso_actuation_object(lwm2m_context_t **lwm2mH)
{
    lwm2m_object_t *tempObj;

    tempObj = lwm2m_malloc(sizeof(*tempObj));

    if (NULL != tempObj)
    {
        _ipso_actuation_instance_t *targetP;

        memset(tempObj, 0, sizeof(*tempObj));

        tempObj->objID = IPSO_ACTUATION_OBJECT_ID;

        targetP = lwm2m_malloc(sizeof(*targetP));
        if (NULL == targetP) return NULL;
        memset(targetP, 0, sizeof(*targetP));

        set_LED_state(true);
        targetP->on_off = false;
        targetP->dimmer = 0;
        targetP->on_time = 0;
        strncpy(targetP->multi_state_out, "---", sizeof("---"));
        strncpy(targetP->app_type, "Red LED", sizeof("Red LED"));

        tempObj->instanceList = LWM2M_LIST_ADD(tempObj->instanceList, targetP);
        /*
         * From a single instance object, two more functions are available.
         * - The first one (createFunc) create a new instance and filled it with the provided informations. If an ID is
         *   provided a check is done for verifying his disponibility, or a new one is generated.
         * - The other one (deleteFunc) delete an instance by removing it from the instance list (and freeing the memory
         *   allocated to it)
         */
        tempObj->readFunc = prv_read;
        tempObj->discoverFunc = prv_discover;
        tempObj->writeFunc = prv_write;
        tempObj->executeFunc = prv_exec;
        tempObj->createFunc = prv_create;

        /*
         * All delete methods for IPSO objects has been commented. Because during the process of bootstrapping leshan sends 
         * "DELETE /" command, that deletes all instances of all objects except of those who doesn't have delete method. 
         * Probably, it's a hack, but it's an easiest way to avoid unwanted instances losing.
        */
        // tempObj->deleteFunc = prv_delete;

        lwm2mContext = lwm2mH;
    }

    return tempObj;
}

void free_ipso_actuation_object(lwm2m_object_t *object)
{
    LWM2M_LIST_FREE(object->instanceList);
    if (object->userData != NULL)
    {
        lwm2m_free(object->userData);
        object->userData = NULL;
    }
    lwm2m_free(object);
}

