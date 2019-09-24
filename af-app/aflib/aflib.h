/*
 * aflib.h
 *
 * hub version of aflib
 *
 * Copyright (C) 2018 Afero, Inc. All rights reserved.
 */
#ifndef __AF_AFLIB_H__
#define __AF_AFLIB_H__

#include <stdint.h>
#include <stdbool.h>
#include <event2/event.h>
#include <af_log.h>

#include "aflib_mcu.h"

typedef af_lib_error_t af_status_t;



af_lib_error_t af_lib_set_event_base(struct event_base *ev);

/*
 * af_lib_set_debug_level
 *
 * sets the debug level of the AfLib library, which specifies how much
 * debugging information is logged to syslog. Set to LOG_INFO to disable
 * debug logging. The debug levels range from LOG_DEBUG1 to LOG_DEBUG4
 * with LOG_DEBUG4 being the most verbose.
 *
 * @param level                     - log verboseness level
 *
 */
void af_lib_set_debug_level(int level);

/*
 * af_lib_shutdown
 *
 * Gracefully shuts down the AfLib library and perform house cleanup
 * (calls af_lib_destory())
 *
 * @param af_lib                    - library to close
 *
 */

void af_lib_shutdown(void);


/*
 * af_lib_destroy
 *
 * Gracefully shuts down the AfLib library only.
 *
 * @param af_lib                    - library to close
 *
 */

void af_lib_destroy(af_lib_t* af_lib);


/* API will not accept attribute values larger than this */
#define MAX_ATTRIBUTE_SIZE                  255


#define aflib_set_debug_level af_lib_set_debug_level

#endif // __AF_AFLIB_H_

