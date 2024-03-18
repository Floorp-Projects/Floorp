/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.intent

import android.content.Intent
import mozilla.components.feature.intent.processing.IntentProcessor
import org.mozilla.fenix.BuildConfig

/**
 * Process public deep links that are coming from external apps.
 */
class ExternalDeepLinkIntentProcessor : IntentProcessor {
    /**
     * Processes the given [Intent] verifying if it is an external deeplink.
     *
     * Adding extra flags if it's a deeplink for opening the app as a separate task from the source app of the intent.
     */
    override fun process(intent: Intent): Boolean {
        val isDeeplink = intent.scheme?.equals(BuildConfig.DEEP_LINK_SCHEME, ignoreCase = true) ?: false
        if (isDeeplink) {
            intent.flags = intent.flags or Intent.FLAG_ACTIVITY_NEW_TASK
            intent.flags = intent.flags or Intent.FLAG_ACTIVITY_CLEAR_TOP
        }
        return isDeeplink
    }
}
