/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CC_DEBUG_H_
#define CC_DEBUG_H_
#include "cc_types.h"
#include <cpr_stdio.h>

extern cc_int32_t VCMDebug;
extern cc_int32_t PLATDebug;

#ifndef PLAT_ERROR
#define PLAT_ERROR        err_msg
#endif

#ifndef VCM_ERR
#define VCM_ERR           err_msg
#endif

#ifndef VCM_DEBUG
#define VCM_DEBUG     if (VCMDebug)    buginf
#endif

#ifndef PLAT_DEBUG
#define PLAT_DEBUG    if (PLATDebug)  buginf
#endif

//DEBUG message prefixes
#define PLAT_F_PREFIX "PLAT : %s : "
#define PLAT_A_F_PREFIX "PLAT : %s : %s :"
#define PLAT_L_C_F_PREFIX "PLAT :  %d/%d : %s : " // line/call/fname as arg
#define VCM_F_PREFIX "VCM : %s : "
#define VCM_A_F_PREFIX "VCM : %s : %s :"
#define VCM_L_C_F_PREFIX "%s :  %d/%d : %s : " // line/call/fname as arg
#define PLAT_F_PREFIX_ARGS(msg_name, func_name) msg_name, func_name
#define PLAT_L_C_F_PREFIX_ARGS(msg_name, line, call_id, func_name) \
    msg_name, line, call_id, func_name


//
#define PLAT_API "PLAT_API" // platform API
#define VCM_API  "VCM_API"  // vcm api

#endif /* CC_DEBUG_H_ */
