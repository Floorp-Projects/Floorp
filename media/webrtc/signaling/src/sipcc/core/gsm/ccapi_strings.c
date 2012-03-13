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

#include "cpr_types.h"
#define __CC_FEATURE_STRINGS__ // this is required to include feature strings.
#define __CC_MESSAGES_STRINGS__ // this is required to include message strings.
#define __CC_CAUSE_STRINGS__   // this is required to include cause strings.
#include "text_strings.h"
#include "ccapi.h"

/**
 * This function will be invoked by debug print functions.
 *
 * @param id - feature id
 *
 * @return feature name string.
 */
const char *cc_feature_name (cc_features_t id)
{
    if ((id <= CC_FEATURE_MIN) || (id >= CC_FEATURE_MAX)) {
        return get_debug_string(GSM_UNDEFINED);
    }

    return cc_feature_names[id - CC_FEATURE_NONE];
}


/**
 * This function will be invoked by debug print functions.
 *
 * @param id - cc msg id
 *
 * @return cc msg name string.
 */
const char *cc_msg_name (cc_msgs_t id)
{
    if ((id <= CC_MSG_MIN) || (id >= CC_MSG_MAX)) {
        return get_debug_string(GSM_UNDEFINED);
    }

    return cc_msg_names[id];
}

/**
 * This function will be invoked by debug print functions.
 *
 * @param id - cc cause id
 *
 * @return cc cause name string.
 */
const char *cc_cause_name (cc_causes_t id)
{
    if ((id <= CC_CAUSE_MIN) || (id >= CC_CAUSE_MAX)) {
        return get_debug_string(GSM_UNDEFINED);
    }

    return cc_cause_names[id - CC_CAUSE_OK];
}

