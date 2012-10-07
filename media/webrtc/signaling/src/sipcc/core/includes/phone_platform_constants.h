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

#ifndef _PHONE_PLATFORM_CONSTANTS_H_
#define _PHONE_PLATFORM_CONSTANTS_H_

// Defines for the various phone models. Note that the device numbers
// appearing after the model name are pre-determined when new phone models
// are added and must correspond to the ones programmed into CCM
#define SIP_FIRMWARE_VERSION              "9.1.1"

// Legacy
#define PHONE_MODEL_NUMBER_7940           "8"
#define PHONE_MODEL_NUMBER_7960           "7"
#define LEGACY_MODEL_7940                 "CP-7940G"
#define LEGACY_MODEL_7960                 "CP-7960G"
#define CCSIP_SIP_7940_USER_AGENT         "CP7940G"
#define CCSIP_SIP_7960_USER_AGENT         "CP7960G"

// Communicator
// Note: The Communicator will send its native device number of 30016
// so that the CCM will recognize it as a CIPC. If talking to an older CCM
// that expects the Communicator to register as a 7970, please define
// _CIPC_7970_ in SIP project file
#ifdef _CIPC_7970_
#define PHONE_MODEL_NUMBER_COMMUNICATOR   "30006"
#else
#define PHONE_MODEL_NUMBER_COMMUNICATOR   "30016"
#endif
#define CCSIP_SIP_COMMUNICATOR_USER_AGENT "SIPIPCommunicator"

// UC core
#define PHONE_MODEL_NUMBER_UCCORE   "358"
#define CCSIP_SIP_UCCORE_USER_AGENT "SIPUCCore"

// 8961
#define PHONE_MODEL_NUMBER_8961           "540"
#define RT_MODEL_8961                     "CP-8961"
#define CCSIP_SIP_8961_USER_AGENT         "CP8961"

// 9951
#define PHONE_MODEL_NUMBER_9951           "537"
#define RT_MODEL_9951                     "CP-9951"
#define CCSIP_SIP_9951_USER_AGENT         "CP9951"

// 9971
#define PHONE_MODEL_NUMBER_9971           "493"
#define RT_MODEL_9971                     "CP-9971"
#define CCSIP_SIP_9971_USER_AGENT         "CP9971"

// 7970
#define PHONE_MODEL_NUMBER_7970           "30006"
#define TNP_MODEL_7970                    "CP-7970G"
#define CCSIP_SIP_7970_USER_AGENT         "CP7970G"

// 7971
#define PHONE_MODEL_NUMBER_7971           "119"
#define TNP_MODEL_7971                    "CP-7971G-GE"
#define CCSIP_SIP_7971_USER_AGENT         "CP7971G-GE"

// 7911
#define PHONE_MODEL_NUMBER_7911           "307"
#define TNP_MODEL_7911                    "CP-7911G"
#define CCSIP_SIP_7911_USER_AGENT         "CP7911G"

// 7906
#define PHONE_MODEL_NUMBER_7906           "369"
#define TNP_MODEL_7906                    "CP-7906G"
#define CCSIP_SIP_7906_USER_AGENT         "CP7906G"

// 7931
#define PHONE_MODEL_NUMBER_7931           "348"
#define TNP_MODEL_7931                    "CP-7931G"
#define CCSIP_SIP_7931_USER_AGENT         "CP7931G"

// Maximum Lines on 794X models
#define MAX_REG_LINES_794X                 2

// 7941
#define PHONE_MODEL_NUMBER_7941           "115"
#define TNP_MODEL_7941                    "CP-7941G"
#define CCSIP_SIP_7941_USER_AGENT         "CP7941G"

// 7941 GE
#define PHONE_MODEL_NUMBER_7941GE         "309"
#define TNP_MODEL_7941GE                  "CP-7941G-GE"
#define CCSIP_SIP_7941_GE_USER_AGENT      "CP7941G-GE"

// 7961
#define PHONE_MODEL_NUMBER_7961           "30018"
#define TNP_MODEL_7961                    "CP-7961G"
#define CCSIP_SIP_7961_USER_AGENT         "CP7961G"

// 7961 GE
#define PHONE_MODEL_NUMBER_7961GE         "308"
#define TNP_MODEL_7961GE                  "CP-7961G-GE"
#define CCSIP_SIP_7961_GE_USER_AGENT      "CP7961G-GE"

// 7942
#define PHONE_MODEL_NUMBER_7942           "434"
#define TNP_MODEL_7942                    "CP-7942G"
#define CCSIP_SIP_7942_USER_AGENT         "CP7942G"

// 7945
#define PHONE_MODEL_NUMBER_7945           "435"
#define TNP_MODEL_7945                    "CP-7945G"
#define CCSIP_SIP_7945_USER_AGENT         "CP7945G"

// 7962
#define PHONE_MODEL_NUMBER_7962           "404"
#define TNP_MODEL_7962                    "CP-7962G"
#define CCSIP_SIP_7962_USER_AGENT         "CP7962G"

// 7965
#define PHONE_MODEL_NUMBER_7965           "436"
#define TNP_MODEL_7965                    "CP-7965G"
#define CCSIP_SIP_7965_USER_AGENT         "CP7965G"

// 7975
#define PHONE_MODEL_NUMBER_7975           "437"
#define TNP_MODEL_7975                    "CP-7975G"
#define CCSIP_SIP_7975_USER_AGENT         "CP7975G"

// CSF
#define PHONE_MODEL_NUMBER_CSF            "503"
#define CSF_MODEL                         "CSF"
#define CCSIP_SIP_CSF_USER_AGENT          "IKRAN"

// CIUS
#define PHONE_MODEL_NUMBER_CIUS           "593"
#define CIUS_MODEL                        "CP-CIUS"
#define CCSIP_SIP_CIUS_USER_AGENT         "CPCIUS"

// SOUNDWAVE
#define PHONE_MODEL_NUMBER_SOUNDWAVE      "575"
#define SOUNDWAVE_MODEL                   "SOUNDWAVE"    
#define CCSIP_SIP_SOUNDWAVE_USER_AGENT    "SOUNDWAVE"




#define MAX_SIDECAR_LINES       28  // Max number of lines supported on sidecars

#define MAX_BKEM_LINES          48  // Max number of lines supported on BKEMs

/****************************************************
 * Start: definitions for vendor's phones
 */
//Definition for 6901
#define PHONE_MODEL_NUMBER_6901      "547"
#define RTLITE_MODEL_6901            "CP-6901"
#define CCSIP_SIP_6901_USER_AGENT    "CP6901"

//Definitions for 6911
#define PHONE_MODEL_NUMBER_6911      "548"
#define RTLITE_MODEL_6911            "CP-6911"
#define CCSIP_SIP_6911_USER_AGENT    "CP6911"

//Definition for 6921
#define PHONE_MODEL_NUMBER_6921      "495"
#define RTLITE_MODEL_6921            "CP-6921"
#define CCSIP_SIP_6921_USER_AGENT    "CP6921"

//Definition for 6941
#define PHONE_MODEL_NUMBER_6941      "496"
#define RTLITE_MODEL_6941            "CP-6941"
#define CCSIP_SIP_6941_USER_AGENT    "CP6941"

//Definition for 6945
#define PHONE_MODEL_NUMBER_6945      "564"
#define RTLITE_MODEL_6945            "CP-6945"
#define CCSIP_SIP_6945_USER_AGENT    "CP6945"

//Definition for 6961 
#define PHONE_MODEL_NUMBER_6961      "497"
#define RTLITE_MODEL_6961            "CP-6961"
#define CCSIP_SIP_6961_USER_AGENT    "CP6961"

//Definition for 7937
#define PHONE_MODEL_NUMBER_7937      "431"
#define RTLITE_MODEL_7937            "7937G"
#define CCSIP_SIP_7937_USER_AGENT    "7937G"
/**
 * End of vendor's definition
 ******************************************************/

//Default set
#define PHONE_MODEL_NUMBER      "493"
#define PHONE_MODEL             "CP-9971"
#define CCSIP_SIP_USER_AGENT    "CP9971"

#define MAX_PHONE_LINES       8
#define MAX_REG_LINES        51
#define MAX_CALLS            51
#define MAX_CALLS_PER_LINE   51

/*
 * MAX_INSTANCES (call_instances) should equal to maximum number of calls
 * allowed by the phone but MAX_CALLS is defined to be 1 more than the 
 * actual maximum capacity. Therefore define MAX_INSTANCES to MAX_CALLS -1
 */
#define MAX_INSTANCES        (MAX_CALLS - 1) /* max number of instance ID */

/* MAX_CONFIG_LINES - java side defined fixed number to 36 for non-Buckfast TNP
 * models so use 36 in call cases. Changing this needs java side change
 */
#define MAX_CONFIG_LINES 51

#endif
