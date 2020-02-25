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

        val modeDependentProcessors = if (settings.openLinksInAPrivateTab) {
            components.analytics.metrics.track(Event.OpenedLink(Event.OpenedLink.Mode.PRIVATE))
            intent.putExtra(HomeActivity.PRIVATE_BROWSING_MODE, true)
            listOf(
                components.intentProcessors.privateCustomTabIntentProcessor,
                components.intentProcessors.privateIntentProcessor
            )
        } else {
            components.analytics.metrics.track(Event.OpenedLink(Event.OpenedLink.Mode.NORMAL))
            intent.putExtra(HomeActivity.PRIVATE_BROWSING_MODE, false)
            listOf(
                components.intentProcessors.customTabIntentProcessor,
                components.intentProcessors.intentProcessor
            )
        }

        val intentProcessors = listOf(components.intentProcessors.migrationIntentProcessor) +
                components.intentProcessors.externalAppIntentProcessors +
                modeDependentProcessors +
                NewTabShortcutIntentProcessor()

        // Explicitly remove the new task and clear task flags (Our browser activity is a single
        // task activity and we never want to start a second task here).
        intent.flags = intent.flags and Intent.FLAG_ACTIVITY_NEW_TASK.inv()
        intent.flags = intent.flags and Intent.FLAG_ACTIVITY_CLEAR_TASK.inv()

        // IntentReceiverActivity is started with the "excludeFromRecents" flag (set in manifest). We
        // do not want to propagate this flag from the intent receiver activity to the browser.
        intent.flags = intent.flags and Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS.inv()

        // Call process for side effects, short on the first that returns true
        intentProcessors.any { it.process(intent) }

        val intentProcessorType =
            components.intentProcessors.getType(intentProcessors.firstOrNull { it.matches(intent) })

        intent.setClassName(applicationContext, intentProcessorType.activityClassName)
        intent.putExtra(HomeActivity.OPEN_TO_BROWSER, intentProcessorType.shouldOpenToBrowser(intent))

        startActivity(intent)
        // must finish() after starting the other activity
        finish()
    }
}
