/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef CC_DEBUG_H_
#define CC_DEBUG_H_
#include "cc_types.h"
#include <cpr_stdio.h>

extern cc_int32_t VCMDebug;
extern cc_int32_t PLATDebug;

#define PLAT_ERROR        err_msg
#define VCM_ERR           err_msg

#define VCM_DEBUG     if (VCMDebug)    buginf
#define PLAT_DEBUG    if (PLATDebug)  buginf

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
