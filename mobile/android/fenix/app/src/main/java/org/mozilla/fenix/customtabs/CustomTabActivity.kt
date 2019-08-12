/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import androidx.navigation.NavDestination
import mozilla.components.browser.session.intent.getSessionId
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.browser.browsingmode.CustomTabBrowsingModeManager
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.theme.CustomTabThemeManager
import java.security.InvalidParameterException

open class CustomTabActivity : HomeActivity() {
    final override fun getSentryBreadcrumbMessage(destination: NavDestination): String {
        val fragmentName = resources.getResourceEntryName(destination.id)
        return "Changing to fragment $fragmentName, isCustomTab: true"
    }

    final override fun getIntentSource(intent: SafeIntent) = Event.OpenedApp.Source.CUSTOM_TAB

    final override fun getIntentSessionId(intent: SafeIntent) = intent.getSessionId()

    override fun getNavDirections(
        from: BrowserDirection,
        customTabSessionId: String?
    ) = when (from) {
        BrowserDirection.FromGlobal ->
            NavGraphDirections.actionGlobalExternalAppBrowser(customTabSessionId)
        else -> throw InvalidParameterException(
            "Tried to navigate to ExternalAppBrowserFragment from $from"
        )
    }

    final override fun createBrowsingModeManager() =
        CustomTabBrowsingModeManager()

    final override fun createThemeManager() = CustomTabThemeManager()
}
