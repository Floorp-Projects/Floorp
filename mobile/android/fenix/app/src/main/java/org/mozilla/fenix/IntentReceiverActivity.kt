/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import org.mozilla.fenix.components.getType
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.shortcut.NewTabShortcutIntentProcessor

/**
 * Processes incoming intents and sends them to the corresponding activity.
 */
class IntentReceiverActivity : Activity() {

    @VisibleForTesting
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        MainScope().launch {
            // The intent property is nullable, but the rest of the code below
            // assumes it is not. If it's null, then we make a new one and open
            // the HomeActivity.
            val intent = intent?.let { Intent(intent) } ?: Intent()
            processIntent(intent)
        }
    }

    suspend fun processIntent(intent: Intent) {
        val settings = settings()
        settings.unsetOpenLinksInAPrivateTabIfNecessary()

        val modeDependentProcessors = if (settings.openLinksInAPrivateTab) {
            components.analytics.metrics.track(Event.OpenedLink(Event.OpenedLink.Mode.PRIVATE))
            listOf(
                components.intentProcessors.privateCustomTabIntentProcessor,
                components.intentProcessors.privateIntentProcessor
            )
        } else {
            components.analytics.metrics.track(Event.OpenedLink(Event.OpenedLink.Mode.NORMAL))
            listOf(
                components.intentProcessors.customTabIntentProcessor,
                components.intentProcessors.intentProcessor
            )
        }

        val intentProcessors = components.intentProcessors.externalAppIntentProcessors +
                modeDependentProcessors +
                NewTabShortcutIntentProcessor()

        // Call process for side effects, short on the first that returns true
        intentProcessors.any { it.process(intent) }

        val intentProcessorType =
            components.intentProcessors.getType(intentProcessors.firstOrNull { it.matches(intent) })

        intent.setClassName(applicationContext, intentProcessorType.activityClassName)
        intent.putExtra(HomeActivity.OPEN_TO_BROWSER, intentProcessorType.shouldOpenToBrowser(intent))

        // finish() before starting another activity. Don't keep this on the activities back stack.
        finish()
        startActivity(intent)
    }
}
