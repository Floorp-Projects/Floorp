/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import android.app.PendingIntent
import android.graphics.Bitmap
import android.os.Bundle
import androidx.annotation.ColorInt
import androidx.browser.customtabs.CustomTabsIntent
import androidx.browser.customtabs.CustomTabsSessionToken

/**
 * Holds configuration data for a Custom Tab.
 *
 * @property id a unique ID of this custom tab.
 * @property toolbarColor Background color for the toolbar.
 * @property closeButtonIcon Custom icon of the back button on the toolbar.
 * @property enableUrlbarHiding Enables the toolbar to hide as the user scrolls down on the page.
 * @property actionButtonConfig Custom action button on the toolbar.
 * @property showCloseButton Specifies whether the close button will be shown on the toolbar.
 * @property showShareMenuItem Specifies whether a default share button will be shown in the menu.
 * @property menuItems Custom overflow menu items.
 * @property exitAnimations Bundle containing custom exit animations for the tab.
 * @property navigationBarColor Background color for the navigation bar.
 * @property titleVisible Whether the title should be shown in the custom tab.
 * @property sessionToken The token associated with the custom tab.
 * @property externalAppType How this custom tab is being displayed.
 */
data class CustomTabConfig(
    @ColorInt val toolbarColor: Int? = null,
    val closeButtonIcon: Bitmap? = null,
    val enableUrlbarHiding: Boolean = false,
    val actionButtonConfig: CustomTabActionButtonConfig? = null,
    val showCloseButton: Boolean = true,
    val showShareMenuItem: Boolean = false,
    val menuItems: List<CustomTabMenuItem> = emptyList(),
    val exitAnimations: Bundle? = null,
    @ColorInt val navigationBarColor: Int? = null,
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
