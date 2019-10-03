/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.feature.intent.processing.TabIntentProcessor
import mozilla.components.support.utils.Browsers
import org.mozilla.fenix.customtabs.AuthCustomTabActivity
import org.mozilla.fenix.customtabs.AuthCustomTabActivity.Companion.EXTRA_AUTH_CUSTOM_TAB
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor

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
        val didLaunchPrivateLink = packageManager
            ?.getActivityInfo(componentName, PackageManager.GET_META_DATA)
            ?.metaData
            ?.getBoolean(LAUNCH_PRIVATE_LINK) ?: false

        /* If LAUNCH_PRIVATE_LINK is set AND we're the default browser they must have pressed "always."
        This is because LAUNCH_PRIVATE_LINK is only accessible through the "launch browser intent" menu
        Which only appears if the user doesn't have a default set. */
        if (didLaunchPrivateLink && Browsers.all(this).isDefaultBrowser) {
            this.settings().openLinksInAPrivateTab = true
        } else if (!Browsers.all(this).isDefaultBrowser) {
            /* If the user has unset us as the default browser, unset alwaysOpenInPrivateMode */
            this.settings().openLinksInAPrivateTab = false
        }

        val tabIntentProcessor = if (settings().openLinksInAPrivateTab || didLaunchPrivateLink) {
            components.intentProcessors.privateIntentProcessor
        } else {
            components.intentProcessors.intentProcessor
        }

        val intentProcessors =
            components.intentProcessors.externalAppIntentProcessors + tabIntentProcessor

        intentProcessors.any { it.process(intent) }
        setIntentActivity(intent, tabIntentProcessor)

        startActivity(intent)

        finish()
    }

    /**
     * Sets the activity that this [intent] will launch.
     */
    private fun setIntentActivity(intent: Intent, tabIntentProcessor: TabIntentProcessor) {
        val openToBrowser = when {
            components.intentProcessors.externalAppIntentProcessors.any { it.matches(intent) } -> {
                // TODO this needs to change: https://github.com/mozilla-mobile/fenix/issues/5225
                val activityClass = if (intent.hasExtra(EXTRA_AUTH_CUSTOM_TAB)) {
                    AuthCustomTabActivity::class
                } else {
                    ExternalAppBrowserActivity::class
                }
                intent.setClassName(applicationContext, activityClass.java.name)
                true
            }
            tabIntentProcessor.matches(intent) -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                // This Intent was launched from history (recent apps). Android will redeliver the
                // original Intent (which might be a VIEW intent). However if there's no active browsing
                // session then we do not want to re-process the Intent and potentially re-open a website
                // from a session that the user already "erased".
                intent.flags and Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY == 0
            }
            intent.action == ACTION_OPEN_TAB || intent.action == ACTION_OPEN_PRIVATE_TAB -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                val startPrivateMode = (intent.action == ACTION_OPEN_PRIVATE_TAB)
                if (startPrivateMode) {
                    intent.putExtra(
                        HomeActivity.OPEN_TO_SEARCH,
                        StartSearchIntentProcessor.STATIC_SHORTCUT_NEW_PRIVATE_TAB
                    )
                } else {
                    intent.putExtra(
                        HomeActivity.OPEN_TO_SEARCH,
                        StartSearchIntentProcessor.STATIC_SHORTCUT_NEW_TAB
                    )
                }
                intent.putExtra(HomeActivity.PRIVATE_BROWSING_MODE, startPrivateMode)
                intent.flags = intent.flags or Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                false
            }
            else -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                false
            }
        }

        intent.putExtra(HomeActivity.OPEN_TO_BROWSER, openToBrowser)
    }

    companion object {
        // This constant must match the metadata from the private activity-alias
        const val LAUNCH_PRIVATE_LINK = "org.mozilla.fenix.LAUNCH_PRIVATE_LINK"

        const val ACTION_OPEN_TAB = "org.mozilla.fenix.OPEN_TAB"
        const val ACTION_OPEN_PRIVATE_TAB = "org.mozilla.fenix.OPEN_PRIVATE_TAB"
    }
}
