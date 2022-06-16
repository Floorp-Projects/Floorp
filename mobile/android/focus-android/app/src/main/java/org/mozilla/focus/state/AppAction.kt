/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.state

import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.Action
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermission

/**
 * An [Action] to be dispatched on the [AppStore].
 */
sealed class AppAction : Action {
    /**
     * The selected tab has changed.
     */
    data class SelectionChanged(
        val tabId: String
    ) : AppAction()

    /**
     * Action for editing the URL of the tab with the given [tabId].
     */
    data class EditAction(
        val tabId: String
    ) : AppAction()

    /**
     * All tabs have been removed.
     */
    object NoTabs : AppAction()

    /**
     * The user finished editing the URL of the tab with the given [tabId].
     */
    data class FinishEdit(
        val tabId: String
    ) : AppAction()

    /**
     * Hide the tabs tray.
     */
    object HideTabs : AppAction()

    /**
     * The user finished the first run onboarding.
     */
    data class FinishFirstRun(val tabId: String?) : AppAction()

    /**
     * The app should get locked.
     */
    object Lock : AppAction()

    /**
     * The app should get unlocked.
     */
    data class Unlock(val tabId: String?) : AppAction()

    data class OpenSettings(val page: Screen.Settings.Page) : AppAction()

    data class OpenSitePermissionOptionsScreen(val sitePermission: SitePermission) : AppAction()

    data class NavigateUp(val tabId: String?) : AppAction()

    /**
     * Forces showing the first run screen.
     */
    internal object ShowFirstRun : AppAction()

    /**
     * Forces showing the home screen.
     */
    internal object ShowHomeScreen : AppAction()

    /**
     * Opens the tab with the given [tabId] and actively switches to the browser screen if needed.
     */
    data class OpenTab(val tabId: String) : AppAction()

    /**
     * The list of [TopSite] has changed.
     */
    data class TopSitesChange(val topSites: List<TopSite>) : AppAction()

    /**
     * Site permissions autoplay rules has changed.
     */
    data class SitePermissionOptionChange(val value: Boolean) : AppAction()

    /**
     * State of secret settings has changed.
     */
    data class SecretSettingsStateChange(val enabled: Boolean) : AppAction()

    /**
     * State of erase tabs CFR has changed
     */
    data class ShowEraseTabsCfrChange(val value: Boolean) : AppAction()

    /**
     * State of show Tracking Protection CFR has changed
     */
    data class ShowTrackingProtectionCfrChange(val value: Map<String, Boolean>) : AppAction()
}
