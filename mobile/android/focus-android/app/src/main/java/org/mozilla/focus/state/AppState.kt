/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.state

import android.os.Bundle
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.State
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermission
import java.util.UUID

/**
 * Global state of the application.
 *
 * @property screen The currently displayed screen.
 * @property topSites The list of [TopSite] to display on the Home screen.
 * @property sitePermissionOptionChange A flag which reflects the state of site permission rules,
 * whether they have been updated or not
 * @property secretSettingsEnabled A flag which reflects the state of debug secret settings
 * @property showEraseTabsCfr A flag which reflects the state erase tabs CFR
 * @property showStartBrowsingTabsCfr A flag which reflects the state of start browsing CFR
 * @property showSearchWidgetSnackbar A flag which reflects the state of search widget snackbar
 * @property showCookieBannerCfr A flag witch reflects the state of cookie banner CFR
 */
data class AppState(
    val screen: Screen,
    val topSites: List<TopSite> = emptyList(),
    val sitePermissionOptionChange: Boolean = false,
    val secretSettingsEnabled: Boolean = false,
    val showEraseTabsCfr: Boolean = false,
    val showSearchWidgetSnackbar: Boolean = false,
    val showTrackingProtectionCfrForTab: Map<String, Boolean> = emptyMap(),
    val showStartBrowsingTabsCfr: Boolean = false,
    val showCookieBannerCfr: Boolean = false,
) : State

/**
 * A group of screens that the app can display.
 *
 * @property id A unique ID for identifying a screen. Only if this ID changes the screen is
 * considered a new screen that requires a navigation (as opposed to the state of the screen
 * changing).
 */
sealed class Screen {
    open val id = UUID.randomUUID().toString()

    /**
     * First run onboarding.
     */
    object FirstRun : Screen()

    /**
     * Second screen from new onboarding flow.
     */
    object OnboardingSecondScreen : Screen()

    /**
     * The home screen.
     */
    object Home : Screen()

    /**
     * Browser screen.
     *
     * @property tabId The ID of the displayed tab.
     * @property showTabs Whether to show the tabs tray.
     */
    data class Browser(
        val tabId: String,
        val showTabs: Boolean,
    ) : Screen() {
        // Whenever the showTabs property changes we want to treat this as a new screen and force
        // a navigation.
        override val id: String = "${super.id}_$showTabs}"
    }

    /**
     * Editing the URL of a tab.
     */
    data class EditUrl(
        val tabId: String,
    ) : Screen()

    /**
     * The application is locked (and requires unlocking).
     *
     * @param bundle it is used for app navigation. If the user can unlock with success he should
     * be redirected to a certain screen.It comes from the external intent.
     */
    data class Locked(val bundle: Bundle? = null) : Screen()
    data class SitePermissionOptionsScreen(val sitePermission: SitePermission) : Screen()
    data class Settings(
        val page: Page = Page.Start,
    ) : Screen() {
        enum class Page {
            Start,

            General,
            Privacy,
            Search,
            Advanced,
            Mozilla,
            About,
            Licenses,
            Locale,

            PrivacyExceptions,
            PrivacyExceptionsRemove,
            CookieBanner,
            SitePermissions,
            Studies,
            SecretSettings,

            SearchList,
            SearchRemove,
            SearchAdd,
            SearchAutocomplete,
            SearchAutocompleteList,
            SearchAutocompleteAdd,
            SearchAutocompleteRemove,
        }
    }
}
