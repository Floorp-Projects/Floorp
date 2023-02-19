/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.StrictMode
import androidx.annotation.VisibleForTesting
import mozilla.components.feature.intent.ext.sanitize
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.support.utils.EXTRA_ACTIVITY_REFERRER_CATEGORY
import mozilla.components.support.utils.EXTRA_ACTIVITY_REFERRER_PACKAGE
import mozilla.components.support.utils.ext.getApplicationInfoCompat
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity.Companion.PRIVATE_BROWSING_MODE
import org.mozilla.fenix.components.IntentProcessorType
import org.mozilla.fenix.components.getType
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.perf.MarkersActivityLifecycleCallbacks
import org.mozilla.fenix.perf.StartupTimeline
import org.mozilla.fenix.shortcut.NewTabShortcutIntentProcessor

/**
 * Processes incoming intents and sends them to the corresponding activity.
 */
class IntentReceiverActivity : Activity() {

    @VisibleForTesting
    override fun onCreate(savedInstanceState: Bundle?) {
        // DO NOT MOVE ANYTHING ABOVE THIS getProfilerTime CALL.
        val startTimeProfiler = components.core.engine.profiler?.getProfilerTime()

        // StrictMode violation on certain devices such as Samsung
        components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            super.onCreate(savedInstanceState)
        }

        // The intent property is nullable, but the rest of the code below
        // assumes it is not. If it's null, then we make a new one and open
        // the HomeActivity.
        val intent = intent?.let { Intent(it) } ?: Intent()
        intent.sanitize().stripUnwantedFlags()
        processIntent(intent)

        components.core.engine.profiler?.addMarker(
            MarkersActivityLifecycleCallbacks.MARKER_NAME,
            startTimeProfiler,
            "IntentReceiverActivity.onCreate",
        )
        StartupTimeline.onActivityCreateEndIntentReceiver() // DO NOT MOVE ANYTHING BELOW HERE.
    }

    fun processIntent(intent: Intent) {
        // Call process for side effects, short on the first that returns true

        var private = settings().openLinksInAPrivateTab
        if (!private) {
            // if PRIVATE_BROWSING_MODE is already set to true, honor that
            private = intent.getBooleanExtra(PRIVATE_BROWSING_MODE, false)
        }
        intent.putExtra(PRIVATE_BROWSING_MODE, private)
        if (private) {
            Events.openedLink.record(Events.OpenedLinkExtra("PRIVATE"))
        } else {
            Events.openedLink.record(Events.OpenedLinkExtra("NORMAL"))
        }

        addReferrerInformation(intent)

        val processor = getIntentProcessors(private).firstOrNull { it.process(intent) }
        val intentProcessorType = components.intentProcessors.getType(processor)

        launch(intent, intentProcessorType)
    }

    @VisibleForTesting
    internal fun launch(intent: Intent, intentProcessorType: IntentProcessorType) {
        intent.setClassName(applicationContext, intentProcessorType.activityClassName)

        if (!intent.hasExtra(HomeActivity.OPEN_TO_BROWSER)) {
            intent.putExtra(
                HomeActivity.OPEN_TO_BROWSER,
                intentProcessorType.shouldOpenToBrowser(intent),
            )
        }
        // StrictMode violation on certain devices such as Samsung
        components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            startActivity(intent)
        }
        finish() // must finish() after starting the other activity
    }

    private fun getIntentProcessors(private: Boolean): List<IntentProcessor> {
        val modeDependentProcessors = if (private) {
            listOf(
                components.intentProcessors.privateCustomTabIntentProcessor,
                components.intentProcessors.privateIntentProcessor,
            )
        } else {
            Events.openedLink.record(Events.OpenedLinkExtra("NORMAL"))
            listOf(
                components.intentProcessors.customTabIntentProcessor,
                components.intentProcessors.intentProcessor,
            )
        }

        return components.intentProcessors.externalAppIntentProcessors +
            components.intentProcessors.fennecPageShortcutIntentProcessor +
            components.intentProcessors.externalDeepLinkIntentProcessor +
            components.intentProcessors.webNotificationsIntentProcessor +
            modeDependentProcessors +
            NewTabShortcutIntentProcessor()
    }

    private fun addReferrerInformation(intent: Intent) {
        // Pass along referrer information when possible.
        // Referrer is supported for API>=22.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP_MR1) {
            return
        }
        // NB: referrer can be spoofed by the calling application. Use with caution.
        val r = referrer ?: return
        intent.putExtra(EXTRA_ACTIVITY_REFERRER_PACKAGE, r.host)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Category is supported for API>=26.
            r.host?.let { host ->
                try {
                    val category = packageManager.getApplicationInfoCompat(host, 0).category
                    intent.putExtra(EXTRA_ACTIVITY_REFERRER_CATEGORY, category)
                } catch (e: PackageManager.NameNotFoundException) {
                    // At least we tried.
                }
            }
        }
    }
}

private fun Intent.stripUnwantedFlags() {
    // Explicitly remove the new task and clear task flags (Our browser activity is a single
    // task activity and we never want to start a second task here).
    flags = flags and Intent.FLAG_ACTIVITY_NEW_TASK.inv()
    flags = flags and Intent.FLAG_ACTIVITY_CLEAR_TASK.inv()

    // IntentReceiverActivity is started with the "excludeFromRecents" flag (set in manifest). We
    // do not want to propagate this flag from the intent receiver activity to the browser.
    flags = flags and Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS.inv()
}
