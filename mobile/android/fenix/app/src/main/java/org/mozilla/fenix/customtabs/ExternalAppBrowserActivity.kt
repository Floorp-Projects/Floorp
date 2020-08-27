/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import mozilla.components.browser.session.runWithSession
import mozilla.components.concept.engine.manifest.WebAppManifestParser
import mozilla.components.feature.intent.ext.getSessionId
import mozilla.components.feature.pwa.ext.getWebAppManifest
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import java.security.InvalidParameterException

/**
 * Activity that holds the [ExternalAppBrowserFragment] that is launched within an external app,
 * such as custom tabs and progressive web apps.
 */
open class ExternalAppBrowserActivity : HomeActivity() {

    final override fun getBreadcrumbMessage(destination: NavDestination): String {
        val fragmentName = resources.getResourceEntryName(destination.id)
        return "Changing to fragment $fragmentName, isCustomTab: true"
    }

    final override fun getIntentSource(intent: SafeIntent) = Event.OpenedApp.Source.CUSTOM_TAB

    final override fun getIntentAllSource(intent: SafeIntent) = Event.AppReceivedIntent.Source.CUSTOM_TAB

    final override fun getIntentSessionId(intent: SafeIntent) = intent.getSessionId()

    override fun startupTelemetryOnCreateCalled(safeIntent: SafeIntent, hasSavedInstanceState: Boolean) {
        components.appStartupTelemetry.onExternalAppBrowserOnCreate(safeIntent, hasSavedInstanceState)
    }

    override fun getNavDirections(
        from: BrowserDirection,
        customTabSessionId: String?
    ): NavDirections? {
        if (customTabSessionId == null) {
            finish()
            return null
        }

        val manifest = intent
            .getWebAppManifest()
            ?.let { WebAppManifestParser().serialize(it).toString() }
        return when (from) {
            BrowserDirection.FromGlobal ->
                NavGraphDirections.actionGlobalExternalAppBrowser(
                    activeSessionId = customTabSessionId,
                    webAppManifest = manifest
                )
            else -> throw InvalidParameterException(
                "Tried to navigate to ExternalAppBrowserFragment from $from"
            )
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        if (isFinishing) {
            // When this activity finishes, the process is staying around and the session still
            // exists then remove it now to free all its resources. Once this activity is finished
            // then there's no way to get back to it other than relaunching it.
            val sessionId = getIntentSessionId(SafeIntent(intent))
            components.core.sessionManager.runWithSession(sessionId) { session ->
                // If the custom tag config has been removed we are opening this in normal browsing
                if (session.customTabConfig != null) {
                    remove(session)
                }
                true
            }
        }
    }
}
