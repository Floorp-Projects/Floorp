/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.mozilla.fenix.R

internal fun SitePermissionsRules.Action.toString(context: Context): String {
    return when (this) {
        SitePermissionsRules.Action.ASK_TO_ALLOW -> {
            context.getString(R.string.preference_option_phone_feature_ask_to_allow)
        }
        SitePermissionsRules.Action.BLOCKED -> {
            context.getString(R.string.preference_option_phone_feature_block)
        }
    }
}
