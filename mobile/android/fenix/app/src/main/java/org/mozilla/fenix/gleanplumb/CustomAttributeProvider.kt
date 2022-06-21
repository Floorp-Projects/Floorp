/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.content.Context
import org.json.JSONObject
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.BrowsersCache
import java.text.SimpleDateFormat
import java.util.Locale
import java.util.Calendar

/**
 * Custom attributes that the messaging framework will use to evaluate if message is eligible
 * to be shown.
 */
object CustomAttributeProvider {
    private val formatter = SimpleDateFormat("yyyy-MM-dd", Locale.US)

    /**
     * Returns a [JSONObject] that contains all the custom attributes, evaluated when the function
     * was called.
     */
    fun getCustomAttributes(context: Context): JSONObject {
        val now = Calendar.getInstance()
        return JSONObject(
            mapOf(
                "is_default_browser_string" to BrowsersCache.all(context).isDefaultBrowser.toString(),
                "date_string" to formatter.format(now.time),
                "number_of_app_launches" to context.settings().numberOfAppLaunches
            )
        )
    }
}
