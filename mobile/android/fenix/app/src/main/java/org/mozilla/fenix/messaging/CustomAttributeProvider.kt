/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import android.content.Context
import androidx.core.app.NotificationManagerCompat
import mozilla.components.service.nimbus.messaging.JexlAttributeProvider
import org.json.JSONObject
import org.mozilla.fenix.components.metrics.UTMParams.Companion.UTM_CAMPAIGN
import org.mozilla.fenix.components.metrics.UTMParams.Companion.UTM_CONTENT
import org.mozilla.fenix.components.metrics.UTMParams.Companion.UTM_MEDIUM
import org.mozilla.fenix.components.metrics.UTMParams.Companion.UTM_SOURCE
import org.mozilla.fenix.components.metrics.UTMParams.Companion.UTM_TERM
import org.mozilla.fenix.ext.areNotificationsEnabledSafe
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.BrowsersCache
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale

/**
 * Custom attributes that the messaging framework will use to evaluate if message is eligible
 * to be shown.
 */
object CustomAttributeProvider : JexlAttributeProvider {
    private val formatter = SimpleDateFormat("yyyy-MM-dd", Locale.US)

    /**
     * Return a [JSONObject] of custom attributes used for experiment targeting.
     *
     * These are only evaluated right at the beginning of start up, so any first run experiments needing
     * targeting attributes which aren't set until after startup e.g. are_notifications_enabled
     * will unlikely to targeted as expected.
     */
    fun getCustomTargetingAttributes(context: Context): JSONObject {
        val isFirstRun = context.settings().isFirstNimbusRun
        return JSONObject(
            mapOf(
                // By convention, we should use snake case.
                "is_first_run" to isFirstRun,

                // This camelCase attribute is a boolean value represented as a string.
                // This is left for backwards compatibility.
                "isFirstRun" to isFirstRun.toString(),
            ),
        )
    }

    /**
     * Returns a [JSONObject] that contains all the custom attributes, evaluated when the function
     * was called.
     *
     * This is used to drive display triggers of messages.
     */
    override fun getCustomAttributes(context: Context): JSONObject {
        val now = Calendar.getInstance()
        val settings = context.settings()
        return JSONObject(
            mapOf(
                "is_default_browser" to BrowsersCache.all(context).isDefaultBrowser,
                "date_string" to formatter.format(now.time),
                "number_of_app_launches" to settings.numberOfAppLaunches,

                "adjust_campaign" to settings.adjustCampaignId,
                "adjust_network" to settings.adjustNetwork,
                "adjust_ad_group" to settings.adjustAdGroup,
                "adjust_creative" to settings.adjustCreative,

                UTM_SOURCE to settings.utmSource,
                UTM_MEDIUM to settings.utmMedium,
                UTM_CAMPAIGN to settings.utmCampaign,
                UTM_TERM to settings.utmTerm,
                UTM_CONTENT to settings.utmContent,

                "are_notifications_enabled" to NotificationManagerCompat.from(context).areNotificationsEnabledSafe(),
            ),
        )
    }
}
