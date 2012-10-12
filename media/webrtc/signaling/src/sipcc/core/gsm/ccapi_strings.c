/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

