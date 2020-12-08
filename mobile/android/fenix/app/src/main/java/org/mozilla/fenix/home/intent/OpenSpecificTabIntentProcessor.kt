/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import mozilla.components.feature.media.service.AbstractMediaService
import mozilla.components.feature.media.service.AbstractMediaSessionService
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FeatureFlags.newMediaSessionApi
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.ext.components

/**
 * When the media notification is clicked we need to switch to the tab where the audio/video is
 * playing. This intent has the following informations:
 * action - [AbstractMediaSessionService.Companion.ACTION_SWITCH_TAB]
 * extra string for the tab id - [AbstractMediaSessionService.Companion.EXTRA_TAB_ID]
 */
class OpenSpecificTabIntentProcessor(
    private val activity: HomeActivity
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        if (intent.action == getAction()) {
            val sessionManager = activity.components.core.sessionManager
            val sessionId = intent.extras?.getString(getTabId())
            val session = sessionId?.let { sessionManager.findSessionById(it) }
            if (session != null) {
                sessionManager.select(session)
                activity.openToBrowser(BrowserDirection.FromGlobal)
                return true
            }
        }

        return false
    }
}

private fun getAction(): String {
    return if (newMediaSessionApi) {
        AbstractMediaSessionService.Companion.ACTION_SWITCH_TAB
    } else {
        AbstractMediaService.Companion.ACTION_SWITCH_TAB
    }
}

private fun getTabId(): String {
    return if (newMediaSessionApi) {
        AbstractMediaSessionService.Companion.EXTRA_TAB_ID
    } else {
        AbstractMediaService.Companion.EXTRA_TAB_ID
    }
}
