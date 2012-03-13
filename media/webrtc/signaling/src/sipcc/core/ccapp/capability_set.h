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

#ifndef _CC_CAPABILITY_SET_H_
#define _CC_CAPABILITY_SET_H_

#include "ccapi_types.h"

extern void capset_get_idleset( cc_cucm_mode_t mode, cc_boolean features[]);
extern void capset_get_allowed_features( cc_cucm_mode_t mode, cc_call_state_t state, cc_boolean features[]);

// FCP (feature control policy) methods
int  fcp_init_template (const char* fcp_plan_string);

// FCP Definitions
#define FCP_MAX_SIZE                0x5000
#define FCP_FEATURE_NAME_MAX        24
#define MAX_FP_VERSION_STAMP_LEN   (64+1)

extern char g_fp_version_stamp[MAX_FP_VERSION_STAMP_LEN];


// for each feature in the XML FCP file, we'll receive the
// feature name, featureId, and whether or not it is enabled
typedef struct cc_feature_control_policy_info_t_ 
{
    char                         featureName[FCP_FEATURE_NAME_MAX];
    unsigned int                 featureId;
    cc_boolean                   featureEnabled;
} cc_feature_control_policy_info_t;

#endif /* _CC_CAPABILITY_SET_H_ */
