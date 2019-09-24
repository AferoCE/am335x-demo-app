/**
   Copyright 2019 Afero, Inc.

*/

// This is a simple unit testing 'EDGED' app, simulating the handling of
// edge attributes (in the hub world) 

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "af_attr_def.h"
#include "af_attr_client.h" // AF_ATTR_MAX_LISTEN_RANGES
#include "af_log.h"
#include "af_rpc.h"
#include "af_ipc_server.h"
#include "aflib/aflib.h"
//#include "aflib/aflib_mcu.h"


af_lib_t          *sAf_lib    = NULL;  // currently not used in hub aflib
struct event_base *sEventBase = NULL;  // we need really this


// ##############
//
// EDGE ATTRIBUTES (or your APP attributes)
//
// EXAMPLE of handle edge/mcu attributes (which should have been defined in the 
// loaded profile on the device): 
//
// attribute with id 1 : led_button, has value on/off or (1/0)
// attribute with id 2 : represents some kind of state value (uint32)

uint8_t   attr_id_1_led_button = 0;  // attributeId = 1
uint32_t  attr_id_2_state      = 0;  // attributeId = 2

// END EDGE ATTRIBUTES
// ##############



// EDGED (or your app): this is what you need to program
//
// This callback is executed any time ASR has information for the MCU
// The name of this event is defined in the aflib initialization in setup()
void attrEventCallback(const af_lib_event_type_t eventType,
                       const af_lib_error_t error,
                       const uint16_t attributeId,
                       const uint16_t valueLen,
                       const uint8_t* value) 
{
    char hexBuf[80];
    bool set_succeeded = 1;
    uint8_t  ret; 

    
    memset(hexBuf, 0, sizeof(hexBuf));
    if (value != NULL) {
        af_util_convert_data_to_hex_with_name("value", (uint8_t *)value, 
                                          valueLen, hexBuf, sizeof(hexBuf));
    }

    AFLOG_INFO("attrEventCallback:attrid:%d, %s, len=%d", attributeId, hexBuf, valueLen);
    switch (eventType) {
        // Unsolicited notification when a non-MCU attribute changes state
        case AF_LIB_EVENT_ASR_NOTIFICATION: // non-edge attribute notify
            AFLOG_INFO("NOTIFICATION EVENT: for attr=%d", attributeId);

            // TESTING: using the notification of the WIFI_RSSI as triggger to
            //   test af_lib_set_attribute_xx call
            //
            // we are interested in attribute wifi rssi (65005) 
            if (attributeId == AF_ATTR_WIFISTAD_WIFI_RSSI) {
               // TESTING: send attributeId =2, with value=8 up
               //   (simulating an edge attribute is changed and need to do a af_lib_set_xx)
               AFLOG_INFO("EDGED: set_attribute_32() for id=2, value=8");
               attr_id_2_state = 8;
               ret = af_lib_set_attribute_32(sAf_lib, 2, 
                                             attr_id_2_state, AF_LIB_SET_REASON_LOCAL_CHANGE);
               if (ret != AF_SUCCESS) {
                   AFLOG_ERR("af_lib_set_attribute: failed set for the test attributeId=2");
               }
            }
            break;


        case AF_LIB_EVENT_ASR_SET_RESPONSE:
            AFLOG_INFO("ASR_SET_RESPONSE EVENT:");
            break;


        case AF_LIB_EVENT_MCU_SET_REQUEST: // edge attribute set request
            AFLOG_INFO("MCU_SET_REQUEST EVENT: for attr=%d", attributeId);
            // in real world scenario:
            // you would know which attribute by switch on (attributeId) and handle the attributes
            // EDGED/Your app needs to handle 
            if (attributeId == 1) {
                attr_id_1_led_button = *value; 
                af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)&attr_id_1_led_button); 
            }
            else if (attributeId == 2) {
                attr_id_2_state = *value; 
                AFLOG_INFO("EDGED: new value=%d", attr_id_2_state);
                af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, 1, (const uint8_t *)&attr_id_2_state); 
            }
            else {
                // Don't know anything about these attributes, reject
                set_succeeded = 0;  // failed the set
                af_lib_send_set_response(sAf_lib, attributeId, set_succeeded, valueLen, value);
            }
            break;


        case AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION: // edge attribute changed
            AFLOG_INFO("EDGE ATTR changed: for attr=%d", attributeId);
            // You code to handle if you are interesed in a particular attribute
            break;


        case AF_LIB_EVENT_ASR_GET_REQUEST: {
            AFLOG_INFO("EDGE ATTR get_reqeust: for attr=%d", attributeId);
            // attribute_store asks for the current value of attribute 
            // (belong to edged or mcu).  Responding with attribute and its value
            // Note 1: af_lib_set_attribute_xx, where xx depends on the type of attributes 

            // Note the following in the call: AF_LIB_SET_REASON_GET_RESPONSE to indicates
            //  it is a reply for the get_request
            if (attributeId == 1) {
                ret = af_lib_set_attribute_8(sAf_lib, attributeId, 
                                             attr_id_1_led_button, AF_LIB_SET_REASON_GET_RESPONSE);
            } 
            else if (attributeId == 2) {
                AFLOG_INFO("af_lib_set_attriubte_32, with AF_LIB_SET_REASON_GET_RESPONSE");
                ret = af_lib_set_attribute_32(sAf_lib, attributeId, 
                                             attr_id_2_state, AF_LIB_SET_REASON_GET_RESPONSE);
            }
            else {
                ret = AF_ERROR_NOT_SUPPORTED;
            }

            if (ret != AF_SUCCESS) {
                AFLOG_ERR("af_lib_set_attribute: failed for request id:%d", attributeId);
            }
            } // AF_LIB_EVENT_ASR_GET_REQUEST
            break;


        default:
           AFLOG_INFO("EVENT: %d, received but not handled", eventType);
           break; 

    } // end switch
}

/* This is the test program to verify aflib implementation */
int main(int argc, char *argv[])
{
    int retVal = AF_SUCCESS;

    printf(" Start pthreads \n");

    /* enable pthreads */
    evthread_use_pthreads();

    printf( "Get new event base pointer.\n");
    /* get an event_base */
    sEventBase = event_base_new();
    if (sEventBase == NULL) {
        AFLOG_ERR("main_event_base_new::can't allocate event base");
        retVal = -1;
        return (retVal);
    }

    AFLOG_INFO("EDGE: start");

    printf(" set the event base for af lib.. \n");

    retVal = af_lib_set_event_base(sEventBase);
    if (retVal != AF_SUCCESS) {
      printf(" set even base failed \n");
        AFLOG_ERR("main_set_event_base::set event base failed");
        goto err_exit;
    }
 
     
    AFLOG_INFO("EDGE: call af_lib_create_with_unified_callback");
    sAf_lib = af_lib_create_with_unified_callback(attrEventCallback, NULL);
    if (sAf_lib == NULL) {
      printf(" cant allocate event base.\n");
        AFLOG_ERR("main_event_base_new::can't allocate event base");
        retVal = -1;
        goto err_exit;
    }
    AFLOG_INFO("EDGE: dispatching event base"); 

    event_base_dispatch(sEventBase);
   

err_exit:
    // done,let's close/clean up 
    AFLOG_INFO("EDGED:  shutdown");
    af_lib_shutdown();
    return (retVal);
}
