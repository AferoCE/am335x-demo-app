/**
 *  Copyright 2019 Afero, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>

#include <af_log.h>
#include <event2/event.h>
#include "af_attr_client.h"

#include "aflib.h"
#include "build_info.h"
#include "af_endian.h"
#include "af_msg_types.h"


static struct event_base        *g_ev = NULL;

// event callback function
static af_lib_event_callback_t  g_event_handler = NULL;

static bool                     g_opened = false;


struct af_lib_t {
    int dummy;
};
af_lib_t g_lib;


/* -------------------------- */
/* aflib4 - aflib release v4  */
/* -------------------------- */
/* We have introduced an extra parameter in the af_lib_set_xxx functions. This
   extra enum parameter indicates if the reason for the set_attribute call.
   HACKY: we need to use a byte arrays (instead of single value) and make a contract
          with hubby about the particular format of the value returned.
   This causes a change in how we format the 'value' field passing into af_attr_set():
   aflib4 only cares about reason when 'EDGED' set the atributes, which hubby uses
   for updating the service.  In the other direction, we ignore the reason.

version=1 uses the following format:
(version and set reason are called META_DATA

   1 byte     1 byte     variable_length
[ version | set reason | data (attribute value ) ]
*/

/* VERSION = 1 */
#define AFLIB_EDGE_FORMAT_VERSION     1

// macro return true if the version in the data is set to AFLIB_EDGE_FORMAT_VERSION
#define IS_AFLIB_VERSION_VALID(ver)   ( (ver) == AFLIB_EDGE_FORMAT_VERSION )

// macro to read the version, reason from the 'value' byte array
#define AFLIB_GET_FORMAT_VERSION(val) ( *(uint8_t *)&val[0] )
#define AFLIB_GET_FORMAT_REASON(val)  ( *(uint8_t *)&val[1] )


/* byte length version, and set reason */
#define AFLIB_EDGE_VERSION_LEN        1
#define AFLIB_EDGE_SET_REASON_LEN     1
#define AFLIB_EDGE_META_DATA_LEN      (AFLIB_EDGE_VERSION_LEN + AFLIB_EDGE_SET_REASON_LEN)


/* convert to attribute store reason */
static uint8_t af_lib_set_reason_converter(af_lib_set_reason_t reason) {
    switch (reason) {
        case AF_LIB_SET_REASON_GET_RESPONSE:
            return UPDATE_REASON_GET_RESPONSE;
        case AF_LIB_SET_REASON_LOCAL_CHANGE:
            return UPDATE_REASON_LOCAL_OR_MCU_UPDATE;
        default:
            return UPDATE_REASON_LOCAL_OR_MCU_UPDATE;
    }
}

/* internal routine formats the byte values array */
static void format_byte_values(const uint16_t       value_len,
                               const uint8_t        *value,
                               af_lib_set_reason_t  set_reason,
                               const uint32_t       value_array_len,
                               uint8_t              *value_array)
{
    uint32_t len = AFLIB_EDGE_META_DATA_LEN + value_len;

    if (len >= value_array_len) {
        value_array[0] = AFLIB_EDGE_FORMAT_VERSION;
        value_array[1] = af_lib_set_reason_converter(set_reason);
        memcpy(&value_array[2], value, value_len);
    }
    else {
        AFLOG_ERR("aflib:format_bytes_value: value array too small, len:%d vs value_array_len:%d", len, value_array_len);
    }
    return;
}
// END aflib v4


/* for af_util: */
uint32_t g_debugLevel = LOG_DEBUG_OFF;

typedef struct outstanding_set {
    struct outstanding_set *next;
    uint16_t attrId;
    uint16_t setId;
} outstanding_set_t;

outstanding_set_t *sSetHead = NULL;

outstanding_set_t *find_and_remove_outstanding_set_with_attr_id(const uint16_t attrId) {
    outstanding_set_t *last = NULL;
    for (outstanding_set_t *s = sSetHead; s; s = s->next) {
        if (s->attrId == attrId) {
            if (last) {
                last->next = s->next;
            } else {
                sSetHead = s->next;
            }
            return s;
        }
        last = s;
    }
    return NULL;
}

typedef struct {
    uint8_t *value;
    uint16_t value_len;
    uint16_t pad;
} set_context_t;

static void on_set_request(uint32_t attrId, uint16_t setId, uint8_t *value, int length, void *context) {
    uint8_t  *edge_data    = value;
    uint8_t  edge_data_len = length;


    if (attrId == AF_ATTR_EDGED_DEBUG_LEVEL && length == 1) {
        g_debugLevel = value[0];
        return; /* not passed on to edge code */
    }
    if (g_event_handler) { /* unified callback */
        outstanding_set_t *s = find_and_remove_outstanding_set_with_attr_id(attrId);
        if (s) {
            AFLOG_ERR("%s_set_exists:setId=%d:ASR has requested to set attribute twice; dropping old request", __func__, s->setId);
            free(s);
        }

        // AFLIB4 format handling for EDGE attributes
        // When other applications such as mobile app, we don't care about the reason
        if (attrId >= AF_ATTR_EDGE_START && attrId <= AF_ATTR_EDGE_END) {
            uint8_t format_ver = AFLIB_GET_FORMAT_VERSION(value);
            if ((length <= AFLIB_EDGE_META_DATA_LEN) || (!IS_AFLIB_VERSION_VALID(format_ver))) {
                char hexBuf[80];
                af_util_convert_data_to_hex_with_name("value", value, length, hexBuf, sizeof(hexBuf));
                AFLOG_ERR("%s_invalid_ver_or_len: drop attrId=%d, setId=%d, %s", __func__, attrId, setId, hexBuf);
                return;
            }
            else {
                // remove the meta-data from the value byte array before we pass it to the 'APP'
                edge_data_len = length - AFLIB_EDGE_META_DATA_LEN;
                edge_data = &value[AFLIB_EDGE_META_DATA_LEN];  // actual value
            }
        }

        s = calloc(1, sizeof(outstanding_set_t));
        if (s) {
            s->attrId = attrId;
            s->setId = setId;
            /* add to set list */
            s->next = sSetHead;
            sSetHead = s;
            g_event_handler(AF_LIB_EVENT_MCU_SET_REQUEST, AF_SUCCESS, attrId, edge_data_len, edge_data);
        }
    } else {
        /* we're assuming that only the service can set MCU attributes, not the ASR */
        int status = AF_ATTR_STATUS_SET_REJECT_BUSY;
        int res = af_attr_send_set_response(status, setId);
        if (res) {
            AFLOG_ERR("%s_send_set_response:res=%d", __func__, res);
        }
    }
}

static void on_get_request(uint32_t attr_id, uint16_t getId, void *context) {

    AFLOG_DEBUG3("AFLIB: on_get_request: %d, %d", attr_id, getId);

    if (attr_id == AF_ATTR_EDGED_DEBUG_LEVEL) {
        uint8_t dl = g_debugLevel;
        af_attr_send_get_response(AF_ATTR_STATUS_OK, getId, &dl, sizeof(dl));
    }
    else {
        AFLOG_DEBUG3("AFLIB: pass the value to the EDGE app");
        g_event_handler(AF_LIB_EVENT_ASR_GET_REQUEST, AF_SUCCESS, attr_id, 0, NULL);
    }
}

static void on_notify(uint32_t attr_id, uint8_t *value, int value_len, void *context) {
    if (attr_id >= AF_ATTR_EDGE_START && attr_id <= AF_ATTR_EDGE_END) {
        if (g_event_handler) {
            g_event_handler(AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION, AF_SUCCESS, attr_id, (uint16_t)value_len, value);
        }
    } else {
        if (g_event_handler) { /* unified callback */
            g_event_handler(AF_LIB_EVENT_ASR_NOTIFICATION, AF_SUCCESS, attr_id, (uint16_t)value_len, value);
        }
    }
}

#define AF_MODULE_STATE_REBOOTED    0
#define AF_MODULE_STATE_LINKED      1
#define AF_MODULE_STATE_INITIALIZED 4

static void schedule_next_open_attempt(void);

static void on_open(int status, void *context) {
    if (status != AF_ATTR_STATUS_OK) {
        schedule_next_open_attempt();
    }
}

static void on_close(int status, void *context) {
    schedule_next_open_attempt();
}

#define ARRAY_SIZE(_x) (sizeof(_x)/sizeof(_x[0]))


static void attempt_to_open(void) {
/*
 51619         : profile has been updated
 65001 - 65024 : wlan system attributes
 65034 - 65037 : wan system attributes
 65060         : eth system attributes
*/
    af_attr_range_t r[] = {
        { 51619, 51619 },
        { 65001, 65024 }, /* FIXME eliminate magic numbers */
        { 65034, 65037 },
        { 65060, 65060 }
    };

    AFLOG_DEBUG3("< attempt_to_open");
    if (g_ev == NULL) {
        AFLOG_ERR("attempt_to_open: failed, event base:g_ev=NULL");
        return;
    }

    int res = af_attr_open(
        g_ev,                   /* base */
        "IPC.EDGED",            /* clientName */
        ARRAY_SIZE(r), r,       /* ranges */
        on_notify,              /* notifyCb */
        on_set_request,         /* ownerSetCb */
        on_get_request,         /* getReqCb */
        on_close,               /* closeCb */
        on_open,                /* openCb */
        NULL);                  /* context */
    if (res != AF_ATTR_STATUS_OK) {
        AFLOG_INFO("AFLIB: open failed, res=%d", res);
        schedule_next_open_attempt();
    }
    AFLOG_DEBUG3("> attempt_to_open");
}

void on_open_timeout(evutil_socket_t fd, short what, void *context) {
    attempt_to_open();
}

void schedule_next_open_attempt(void) {
    struct timeval delay = { 5, 0 };
    event_base_once(g_ev, -1, EV_TIMEOUT, on_open_timeout, NULL, &delay);
}


af_lib_t *af_lib_create(attr_set_handler_t attr_set, attr_notify_handler_t attr_notify, af_transport_t *the_transport) {
    AFLOG_ERR("af_lib_create_unsupported::libafedge does not support afLib2 API.");
    return NULL;
}

af_lib_t *af_lib_create_with_unified_callback(af_lib_event_callback_t event_handler, af_transport_t *transport) {
    if (g_ev == NULL) {
        AFLOG_ERR("%s_event_base::attempted to open afLib without setting event base", __func__);
        return NULL;
    }

    g_event_handler = event_handler;

    attempt_to_open();

    g_opened = true;

    //Note: this is crashing the edged because of BUILD_DATE,REVISION
    // (comment it out for now)
    //AFLOG_INFO("libafedge-unified-%s: %s %s", BUILD_NUMBER, BUILD_DATE, REVISION);
    AFLOG_INFO("libafedge-unified-created: build-%s", BUILD_NUMBER);

    return &g_lib;
}

af_lib_error_t attrd_status_to_aflib_err(int a_status) {
    af_lib_error_t ret = AF_ERROR_UNKNOWN;
    switch (a_status) {
        case AF_ATTR_STATUS_OK :
            ret = AF_SUCCESS;
            break;
        case AF_ATTR_STATUS_ATTR_ID_NOT_FOUND :
            ret = AF_ERROR_NO_SUCH_ATTRIBUTE;
            break;
        case AF_ATTR_STATUS_BAD_DATA :
            ret = AF_ERROR_INVALID_DATA;
            break;
        case AF_ATTR_STATUS_FORBIDDEN :
            ret = AF_ERROR_FORBIDDEN;
            break;
        default :
            AFLOG_ERR("%s_unknown:a_status=%d", __func__, a_status);
            break;
    }
    return ret;
}

void on_get_response(int status, uint32_t attr_id, uint8_t *value, int length, void *context) {
    if (g_event_handler) {
        g_event_handler(AF_LIB_EVENT_ASR_GET_RESPONSE, attrd_status_to_aflib_err(status), (uint16_t)attr_id, (uint16_t)length, value);
    }
    AFLOG_INFO("on_get_response: exit");
}

af_lib_error_t af_lib_get_attribute(af_lib_t *l, const uint16_t attr_id) {
    if (!l) {
        AFLOG_ERR("%s_param:lib_NULL=%d", __func__, l==NULL);
        return AF_ERROR_INVALID_PARAM;
    }
    return attrd_status_to_aflib_err(af_attr_get(attr_id, on_get_response, NULL));
}


void on_set_response(int status, uint32_t attr_id, void *context) {
    set_context_t *s = (set_context_t *)context;
    uint8_t *value = s ? s->value : NULL;
    uint16_t value_len = s ? s->value_len : 0;

    if (g_event_handler) {
        if (attr_id >= AF_ATTR_EDGE_START && attr_id <= AF_ATTR_EDGE_END) {
            if (status != AF_ATTR_STATUS_OK) {
                /* set rejections are only sent if the set failed */
                g_event_handler(AF_LIB_EVENT_MCU_SET_REQ_REJECTION, attrd_status_to_aflib_err(status), (uint16_t)attr_id, value_len, value);
            }
        } else {
            g_event_handler(AF_LIB_EVENT_ASR_SET_RESPONSE, attrd_status_to_aflib_err(status), (uint16_t)attr_id, value_len, value);
        }
    }

    /* free the set context */
    if (s) {
        free(s);
    }
}



af_lib_error_t af_lib_set_attribute_bytes(af_lib_t *l, const uint16_t attr_id, const uint16_t value_len, const uint8_t *value, af_lib_set_reason_t reason) {
    if (!l) {
        AFLOG_ERR("%s_param:lib_NULL=%d", __func__, l==NULL);
        return AF_ERROR_INVALID_PARAM;
    }
    if (value_len > MAX_ATTRIBUTE_SIZE || (value_len > 0 && value == NULL)) {
        AFLOG_ERR("%s_param:value_len=%d,value_NULL=%d", __func__, value_len, value==NULL);
        return AF_ERROR_INVALID_PARAM;
    }

    /* allocate a set context */
    set_context_t *s = NULL;
    if (value_len) {
        s = (set_context_t *)malloc(sizeof(set_context_t) + value_len);
        if (s) {
            s->value = (uint8_t *)s + sizeof(set_context_t);
            memcpy(s->value, value, value_len);
            s->value_len = value_len;
        }
    }

    af_lib_error_t ret = attrd_status_to_aflib_err(af_attr_set(attr_id, (uint8_t *)value, value_len, on_set_response, s));

    /* set request sent */
    if (attr_id >= AF_ATTR_EDGE_START && attr_id <= AF_ATTR_EDGE_END && g_event_handler) {
        // when this is not a reply to a get request
        if (reason != AF_LIB_SET_REASON_GET_RESPONSE) {
            g_event_handler(AF_LIB_EVENT_MCU_SET_REQ_SENT, AF_SUCCESS, attr_id, value_len, value);
        }
    }
    return ret;
}

af_lib_error_t af_lib_set_attribute_bool(af_lib_t *l, const uint16_t attr_id, const bool value, af_lib_set_reason_t reason) {
    uint8_t byte_val = value ? 1 : 0;
    uint8_t len = AFLIB_EDGE_META_DATA_LEN + 1;
    uint8_t byte_values[len];

    format_byte_values(1, &byte_val, reason, len, (uint8_t *)&byte_values[0]);
    return af_lib_set_attribute_bytes(l, attr_id, sizeof(byte_values), &byte_values[0], reason);
}

af_lib_error_t af_lib_set_attribute_8(af_lib_t *l, const uint16_t attr_id, const int8_t value, af_lib_set_reason_t reason) {
    uint8_t len = AFLIB_EDGE_META_DATA_LEN + 1;
    uint8_t byte_values[len];

    format_byte_values(1, (const uint8_t *)&value, reason, len, (uint8_t *)&byte_values[0]);
    return af_lib_set_attribute_bytes(l, attr_id, len, (const uint8_t *)&byte_values[0], reason);
}

af_lib_error_t af_lib_set_attribute_16(af_lib_t *l, const uint16_t attr_id, const int16_t value, af_lib_set_reason_t reason) {
    int16_t i16 = af_htole16(value);
    uint8_t len = AFLIB_EDGE_META_DATA_LEN + sizeof(value);
    uint8_t byte_values[len];

    format_byte_values(sizeof(value), (const uint8_t *)&i16, reason, len, (uint8_t *)&byte_values[0]);
    return af_lib_set_attribute_bytes(l, attr_id, len, (const uint8_t *)&byte_values[0], reason);
}

af_lib_error_t af_lib_set_attribute_32(af_lib_t *l, const uint16_t attr_id, const int32_t value, af_lib_set_reason_t reason) {
    int32_t i32 = af_htole32(value);
    uint8_t len = AFLIB_EDGE_META_DATA_LEN + sizeof(value);
    uint8_t byte_values[len];

    format_byte_values(sizeof(value), (const uint8_t *)&i32, reason, len, (uint8_t *)&byte_values[0]);
    return af_lib_set_attribute_bytes(l, attr_id, len, (const uint8_t *)&byte_values[0], reason);
}

af_lib_error_t af_lib_set_attribute_64(af_lib_t *l, const uint16_t attr_id, const int64_t value, af_lib_set_reason_t reason) {
    int64_t i64 = af_htole64(value);
    uint8_t len = AFLIB_EDGE_META_DATA_LEN + sizeof(value);
    uint8_t byte_values[len];

    format_byte_values(sizeof(value), (const uint8_t *)&i64, reason, len, (uint8_t *)&byte_values[0]);
    return af_lib_set_attribute_bytes(l, attr_id, len, (const uint8_t *)&byte_values[0], reason);
}

af_lib_error_t af_lib_set_attribute_str(af_lib_t            *l,
                                        const uint16_t      attr_id,
                                        const uint16_t      value_len,
                                        const char          *value,
                                        af_lib_set_reason_t reason)
{
    uint8_t len = AFLIB_EDGE_META_DATA_LEN + value_len;
    uint8_t byte_values[len];

    format_byte_values(value_len, (const uint8_t *)value, reason, len, (uint8_t *)&byte_values[0]);
    return af_lib_set_attribute_bytes(l, attr_id, len, (const uint8_t *)&byte_values[0], reason);
}

af_lib_error_t af_lib_send_set_response(af_lib_t *l, const uint16_t attr_id, bool set_succeeded, const uint16_t value_len, const uint8_t *value) {
    outstanding_set_t *s = find_and_remove_outstanding_set_with_attr_id(attr_id);
    int ret = AF_ERROR_INVALID_PARAM;
    if (s) {
        uint16_t setId = s->setId;
        free(s);
        ret = attrd_status_to_aflib_err(af_attr_send_set_response(set_succeeded ? AF_ATTR_STATUS_OK : AF_ATTR_STATUS_SET_REJECT_BUSY, setId));
    }
    return ret;
}

af_lib_error_t af_lib_asr_has_capability(af_lib_t *l, uint32_t af_asr_capability) {
    if (l == NULL) {
        return AF_ERROR_INVALID_PARAM;
    }
    /* we don't support anything */
    return AF_ERROR_NOT_SUPPORTED;
}


void af_lib_shutdown(void) {
    if (!g_opened) {
        AFLOG_INFO("%s_not_open; ignoring", __func__);
    } else {
        af_lib_destroy(&g_lib);
        g_opened = false;
    }
}

void af_lib_destroy(af_lib_t *l) {
    if (!l) {
        AFLOG_ERR("%s_param:l_NULL=%d", __func__, l==NULL);
    } else {
        af_attr_close();
        g_event_handler = NULL;
    }
}

af_lib_error_t af_lib_set_event_base(struct event_base *ev) {
    if (ev == NULL) {
        return AF_ERROR_INVALID_PARAM;
    }
    g_ev = ev;
    return AF_SUCCESS;
}

void af_lib_set_debug_level(int level) {
    g_debugLevel = level;
}

void af_lib_dump_queue() {
    AFLOG_INFO("af_edge version of afLib does not use a queue; no contents to dump");
}
