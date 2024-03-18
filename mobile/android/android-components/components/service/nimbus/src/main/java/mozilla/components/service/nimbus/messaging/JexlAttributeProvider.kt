/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.messaging

import android.content.Context
import org.json.JSONObject

/**
 * A provider that will be used to evaluate if message is eligible to be shown.
 */
interface JexlAttributeProvider {
    /**
     * Returns a [JSONObject] that contains all the custom attributes, evaluated when the function
     * was called.
     *
     * This is used to drive display triggers of messages.
     */
    fun getCustomAttributes(context: Context): JSONObject
}
