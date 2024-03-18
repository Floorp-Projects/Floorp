/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import androidx.preference.PreferenceManager
import mozilla.components.service.glean.config.Configuration
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R

/**
 * Get custom Glean server URL if available.
 */
fun getCustomGleanServerUrlIfAvailable(context: Context): String? {
    return if (BuildConfig.GLEAN_CUSTOM_URL.isNullOrEmpty()) {
        PreferenceManager.getDefaultSharedPreferences(context).getString(
            context.getPreferenceKey(R.string.pref_key_custom_glean_server_url),
            null,
        )
    } else {
        BuildConfig.GLEAN_CUSTOM_URL
    }
}

/**
 * Applies the custom Glean server URL to the Configuration if available.
 */
fun Configuration.setCustomEndpointIfAvailable(serverEndpoint: String?): Configuration {
    if (!serverEndpoint.isNullOrEmpty()) {
        return copy(serverEndpoint = serverEndpoint)
    }

    return this
}
