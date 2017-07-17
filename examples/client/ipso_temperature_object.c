/*
 * Implements an object for temperature sensor
 * This object model is based on IPSO Smart Object design Pattern
 * See this document for more information about IPSO Smart Object: http://www.ipso-alliance.org/wp-content/uploads/2016/01/ipso-paper.pdf
 * To get the IPSO Starter Pack visit this page : http://www.ipso-alliance.org/so-starter-pack/
 *
 *                              Multiple
 *       Object       |  ID  | Instances | Mandatory |               Description              |
 *  IPSO Temperature  | 3303 |    Yes    |    No     | Temperature sensor, example units = Cel|
 *
 *  Resources:
 *              Supported    Multiple
 *  Name    | ID | Operations | Instances | Mandatory |  Type   | Range | Units | Description |
 *  Sensor  |5700|     R      |    No     | Yes       | Float   | 0-255 |       |             |
 *  Value   |    |            |           |           |         |       |       |             | 
 *  Units   |5701|     R      |    No     | Optional  | String  |       |       |             |
 *  Min     |5601|     R      |    No     | Optional  | Float   |       |       |             |
 *  Measured|    |            |           |           |         |       |       |             |
 *  Value   |    |            |           |           |         |       |       |             |
 *  Max     |5602|     R      |    No     | Optional  | Float   |       |       |             |
 *  Measured|    |            |           |           |         |       |       |             |
 *  Value   |    |            |           |           |         |       |       |             |
 *  Min     |5603|     R      |    No     | Optional  | Float   |       |       |             |
 *  Range   |    |            |           |           |         |       |       |             |
 *  Value   |    |            |           |           |         |       |       |             |
 *  Max     |5604|     R      |    No     | Optional  | Float   |       |       |             |
 *  Range   |    |            |           |           |         |       |       |             |
 *  Value   |    |            |           |           |         |       |       |             |
 *  Reset   |5605|     E      |    No     | Optional  | Opaque  |       |       |             |
 *  Min and |    |            |           |           |         |       |       |             |
 *  Max     |    |            |           |           |         |       |       |             |
 *	Measured|    |            |           |           |         |       |       |             |
 *	Values  |    |            |           |           |         |       |       |             |
 *
 */

#include "lwm2mclient.h"

#ifdef ESP8266
#include "periphery.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SENSOR_VALUE_ID 5700
#define UNITS_ID 5701
#define MIN_MEASURED_VALUE_ID 5601
#define MAX_MEASURED_VALUE_ID 5602
#define MIN_RANGE_VALUE_ID 5603
#define MAX_RANGE_VALUE_ID 5604
#define RESET_MIN_AND_MAX_MESURED_VALUES_ID 5605 

#define DEFAULT_MEASURED_VALUE -1

static lwm2m_context_t **lwm2mContext;        // pointer to the lwm2m context object

/*
 * Multiple instance objects can use userdata to store data that will be shared between the different instances.
 * The lwm2m_object_t object structure - which represent every object of the liblwm2m as seen in the single instance
 * object - contain a chained list called instanceList with the object specific structure _ipso_temperature_instance_t:
 */
typedef struct
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _prv_instance_ *next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    double sens_value;
    char units[5];
    double max_measured_value;
    double min_measured_value;
    double max_range_value;
    double min_range_value;
} _ipso_temperature_instance_t;

static uint8_t prv_read(uint16_t instanceId,
                        int *numDataP,
                        lwm2m_data_t **dataArrayP,
                        lwm2m_object_t *objectP)
{
    _ipso_temperature_instance_t *targetP;
    double temp_temperature_value;

    targetP = (_ipso_temperature_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(6);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 6;
        (*dataArrayP)[0].id = SENSOR_VALUE_ID;
        (*dataArrayP)[1].id = UNITS_ID;
        (*dataArrayP)[2].id = MIN_MEASURED_VALUE_ID;
        (*dataArrayP)[3].id = MAX_MEASURED_VALUE_ID;
        (*dataArrayP)[4].id = MIN_RANGE_VALUE_ID;
        (*dataArrayP)[5].id = MAX_RANGE_VALUE_ID;
    }

    for (int i = 0 ; i < *numDataP ; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
        case SENSOR_VALUE_ID:
           temp_temperature_value = (double) get_temperature();

            if (temp_temperature_value != -1)
            {
                if ((targetP->min_measured_value == -1) || (temp_temperature_value < targetP->min_measured_value)) 
                {
                    targetP->min_measured_value = temp_temperature_value;
                }

                if ((targetP->max_measured_value == -1) || (temp_temperature_value > targetP->max_measured_value)) 
                {
                    targetP->max_measured_value = temp_temperature_value;
                }
            }

            targetP->sens_value = temp_temperature_value;

            lwm2m_data_encode_float(targetP->sens_value, *dataArrayP + i);
            break;
        case UNITS_ID:
            lwm2m_data_encode_string(targetP->units, *dataArrayP + i);
            break;
        case MIN_MEASURED_VALUE_ID:
            lwm2m_data_encode_float(targetP->min_measured_value, *dataArrayP + i);
            break;
        case MAX_MEASURED_VALUE_ID:
            lwm2m_data_encode_float(targetP->max_measured_value, *dataArrayP + i);
            break;
        case MIN_RANGE_VALUE_ID:
            lwm2m_data_encode_float(targetP->min_range_value, *dataArrayP + i);
            break;
        case MAX_RANGE_VALUE_ID:
            lwm2m_data_encode_float(targetP->max_range_value, *dataArrayP + i);
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
    (void)objectP;;
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
            SENSOR_VALUE_ID,
            UNITS_ID,
            MIN_MEASURED_VALUE_ID,
            MAX_MEASURED_VALUE_ID,
            MIN_RANGE_VALUE_ID,
            MAX_RANGE_VALUE_ID,
            RESET_MIN_AND_MAX_MESURED_VALUES_ID
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
            case SENSOR_VALUE_ID:
            case UNITS_ID:
            case MIN_MEASURED_VALUE_ID:
            case MAX_MEASURED_VALUE_ID:
            case MIN_RANGE_VALUE_ID:
            case MAX_RANGE_VALUE_ID:
            case RESET_MIN_AND_MAX_MESURED_VALUES_ID:
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
    (void)instanceId;
    (void)numData;
    (void)dataArray;
    (void)objectP;

    return COAP_405_METHOD_NOT_ALLOWED;
}

static uint8_t prv_delete(uint16_t id,
                          lwm2m_object_t *objectP)
{
    _ipso_temperature_instance_t *targetP;

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
    _ipso_temperature_instance_t *targetP;
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
    (void)buffer;
    char uriBuffer[20] = {0};
    lwm2m_uri_t uri;
    _ipso_temperature_instance_t *targetP;

    if (length != 0) return COAP_400_BAD_REQUEST;

    targetP = (_ipso_temperature_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;


    switch (resourceId)
    {
    case RESET_MIN_AND_MAX_MESURED_VALUES_ID:
        targetP->min_measured_value = DEFAULT_MEASURED_VALUE;
        targetP->max_measured_value = DEFAULT_MEASURED_VALUE;
        
        snprintf(uriBuffer, sizeof(uriBuffer), "/%d/%d/%d", IPSO_TEMPERATURE_OBJECT_ID, instanceId, MIN_MEASURED_VALUE_ID);

        lwm2m_stringToUri(uriBuffer, sizeof(uriBuffer), &uri);
        lwm2m_resource_value_changed(*lwm2mContext, &uri);

        snprintf(uriBuffer, sizeof(uriBuffer), "/%d/%d/%d", IPSO_TEMPERATURE_OBJECT_ID, instanceId, MAX_MEASURED_VALUE_ID);

        lwm2m_stringToUri(uriBuffer, sizeof(uriBuffer), &uri);
        lwm2m_resource_value_changed(*lwm2mContext, &uri);



        return COAP_204_CHANGED;
    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

void display_ipso_temperature_object(lwm2m_object_t *object)
{
#ifdef WITH_LOGS
    fprintf(stdout, "  /%u: Temperature object, instances:\r\n", object->objID);
    _ipso_temperature_instance_t *instance = (_ipso_temperature_instance_t *)object->instanceList;

    while (instance != NULL)
    {
        fprintf(stdout, "    /%u/%u: shortId: %u, sens_value: %f, units: %s, max_measured_value: %f," 
                " min_measured_value: %f, max_range_value: %f, min_range_value: %f\n",
                object->objID, instance->shortID,
                instance->shortID, instance->sens_value, instance->units,
                instance->max_measured_value, instance->min_measured_value, 
                instance->max_range_value, instance->min_range_value);

        instance = (_ipso_temperature_instance_t *)instance->next;
    }
#endif
}

lwm2m_object_t *get_ipso_temperature_object(lwm2m_context_t **lwm2mH)
{
    lwm2m_object_t *tempObj;

    tempObj = lwm2m_malloc(sizeof(*tempObj));

    if (NULL != tempObj)
    {
        _ipso_temperature_instance_t *targetP;

        memset(tempObj, 0, sizeof(*tempObj));

        tempObj->objID = IPSO_TEMPERATURE_OBJECT_ID;

        targetP = lwm2m_malloc(sizeof(*targetP));
        if (NULL == targetP) return NULL;
        memset(targetP, 0, sizeof(*targetP));

        targetP->sens_value = get_temperature();
        targetP->sens_value = (double)time(NULL);
        strncpy(targetP->units, "cel", sizeof("cel"));
        targetP->max_measured_value = DEFAULT_MEASURED_VALUE;
        targetP->min_measured_value = DEFAULT_MEASURED_VALUE;
//        targetP->max_range_value = DHT11_max_temp_range_value;
//        targetP->min_range_value = DHT11_min_temp_range_value;

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

void free_ipso_temperature_object(lwm2m_object_t *object)
{
    LWM2M_LIST_FREE(object->instanceList);
    if (object->userData != NULL)
    {
        lwm2m_free(object->userData);
        object->userData = NULL;
    }
    lwm2m_free(object);
}

