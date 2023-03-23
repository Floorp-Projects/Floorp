/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.manifest.WebAppManifestParser
import mozilla.components.feature.intent.ext.getSessionId
import mozilla.components.feature.pwa.ext.getWebAppManifest
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.ext.components
import java.security.InvalidParameterException

const val EXTRA_IS_SANDBOX_CUSTOM_TAB = "org.mozilla.fenix.customtabs.EXTRA_IS_SANDBOX_CUSTOM_TAB"

/**
 * Activity that holds the [ExternalAppBrowserFragment] that is launched within an external app,
 * such as custom tabs and progressive web apps.
 */
@Suppress("TooManyFunctions")
open class ExternalAppBrowserActivity : HomeActivity() {
    override fun onResume() {
        super.onResume()

        if (!hasExternalTab()) {
            // An ExternalAppBrowserActivity is always bound to a specific tab. If this tab doesn't
            // exist anymore on resume then this activity has nothing to display anymore. Let's just
            // finish it AND remove this task to avoid it hanging around in the recent apps screen.
            // Without this the parent HomeActivity class may decide to show the browser UI and we
            // end up with multiple browsers (causing "display already acquired" crashes).
            finishAndRemoveTask()
        }
    }

    final override fun getBreadcrumbMessage(destination: NavDestination): String {
        val fragmentName = resources.getResourceEntryName(destination.id)
        return "Changing to fragment $fragmentName, isCustomTab: true"
    }

    final override fun getIntentSource(intent: SafeIntent) = "CUSTOM_TAB"

    final override fun getIntentSessionId(intent: SafeIntent) = intent.getSessionId()

    override fun navigateToBrowserOnColdStart() {
        // No-op for external app
    }

    override fun navigateToHome() {
        // No-op for external app
    }

    override fun handleNewIntent(intent: Intent) {
        // No-op for external app
    }

    override fun getNavDirections(
        from: BrowserDirection,
        customTabSessionId: String?,
    ): NavDirections? {
        if (customTabSessionId == null) {
            finishAndRemoveTask()
            return null
        }

        val manifest = intent
            .getWebAppManifest()
            ?.let { WebAppManifestParser().serialize(it).toString() }
        return when (from) {
            BrowserDirection.FromGlobal ->
                NavGraphDirections.actionGlobalExternalAppBrowser(
                    activeSessionId = customTabSessionId,
                    webAppManifest = manifest,
                    isSandboxCustomTab = intent.getBooleanExtra(EXTRA_IS_SANDBOX_CUSTOM_TAB, false),
                )
            else -> throw InvalidParameterException(
                "Tried to navigate to ExternalAppBrowserFragment from $from",
            )
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        if (isFinishing) {
            // When this activity finishes, the process is staying around and the session still
            // exists then remove it now to free all its resources. Once this activity is finished
            // then there's no way to get back to it other than relaunching it.
            val tabId = getExternalTabId()
            val customTab = tabId?.let { components.core.store.state.findCustomTab(it) }
            if (tabId != null && customTab != null) {
                components.useCases.customTabsUseCases.remove(tabId)
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun hasExternalTab(): Boolean {
        return getExternalTab() != null
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExternalTab(): SessionState? {
        val id = getExternalTabId() ?: return null
        return components.core.store.state.findCustomTab(id)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExternalTabId(): String? {
        return getIntentSessionId(SafeIntent(intent))
    }
}
