/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import android.app.PendingIntent
import android.graphics.Bitmap
import android.os.Bundle
import androidx.annotation.ColorInt
import androidx.browser.customtabs.CustomTabsIntent
import androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK
import androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT
import androidx.browser.customtabs.CustomTabsIntent.ColorScheme
import androidx.browser.customtabs.CustomTabsSessionToken

/**
 * Holds configuration data for a Custom Tab.
 *
 * @property colorScheme Optional [ColorScheme] to apply to the Custom Tab UI.
 * @property colorSchemes Optional collection of [ColorSchemeParams] for each [ColorScheme].
 * @property closeButtonIcon Optional custom icon of the back button on the toolbar.
 * @property enableUrlbarHiding Enables the toolbar to hide as the user scrolls down on the page.
 * @property actionButtonConfig Optional custom action button on the toolbar.
 * @property showCloseButton Specifies whether the close button will be shown on the toolbar.
 * @property showShareMenuItem Specifies whether a default share button will be shown in the menu.
 * @property menuItems Custom overflow menu items.
 * @property exitAnimations Optional [Bundle] containing custom exit animations for the tab.
 * @property titleVisible Whether the title should be shown in the custom tab.
 * @property sessionToken Optional token associated with the custom tab.
 * @property externalAppType How this custom tab is being displayed.
 */
data class CustomTabConfig(
    @ColorScheme val colorScheme: Int? = null,
    val colorSchemes: ColorSchemes? = null,
    val closeButtonIcon: Bitmap? = null,
    val enableUrlbarHiding: Boolean = false,
    val actionButtonConfig: CustomTabActionButtonConfig? = null,
    val showCloseButton: Boolean = true,
    val showShareMenuItem: Boolean = false,
    val menuItems: List<CustomTabMenuItem> = emptyList(),
    val exitAnimations: Bundle? = null,
    val titleVisible: Boolean = false,
    val sessionToken: CustomTabsSessionToken? = null,
    val externalAppType: ExternalAppType = ExternalAppType.CUSTOM_TAB,
)

/**
 * Represents different contexts that a custom tab session can be displayed in.
 */
enum class ExternalAppType {
    /**
     * Custom tab is displayed as a normal custom tab with toolbar.
     */
    CUSTOM_TAB,

    /**
     * Custom tab toolbar is hidden inside a Progressive Web App created by the browser.
     */
    PROGRESSIVE_WEB_APP,

    /**
     * Custom tab is displayed fullscreen inside a Trusted Web Activity from an external app.
     */
    TRUSTED_WEB_ACTIVITY,
}

data class CustomTabActionButtonConfig(
    val description: String,
    val icon: Bitmap,
    val pendingIntent: PendingIntent,
    val id: Int = CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID,
    val tint: Boolean = false,
)

data class CustomTabMenuItem(
    val name: String,
    val pendingIntent: PendingIntent,
)

/**
 * Holds color data for Custom Tab visual elements.
 *
 * @property toolbarColor Optional background color for the toolbar.
 * @property toolbarColor Optional background color for the secondary toolbar.
 * @property navigationBarColor Optional background color for the navigation bar.
 * @property navigationBarDividerColor Optional background color for the navigation bar divider.
 */
data class ColorSchemeParams(
    @ColorInt val toolbarColor: Int? = null,
    @ColorInt val secondaryToolbarColor: Int? = null,
    @ColorInt val navigationBarColor: Int? = null,
    @ColorInt val navigationBarDividerColor: Int? = null,
)

/**
 * Holds the [ColorSchemeParams] for each possible color scheme.
 *
 * @property defaultColorSchemeParams Optional default [ColorSchemeParams].
 * @property lightColorSchemeParams Optional [ColorSchemeParams] for [COLOR_SCHEME_LIGHT].
 * @property darkColorSchemeParams Optional [ColorSchemeParams] for [COLOR_SCHEME_DARK].
 */
data class ColorSchemes(
    val defaultColorSchemeParams: ColorSchemeParams? = null,
    val lightColorSchemeParams: ColorSchemeParams? = null,
    val darkColorSchemeParams: ColorSchemeParams? = null,
)
