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
import java.util.UUID

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
 */
data class CustomTabConfig(
    val id: String = UUID.randomUUID().toString(),
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
    val sessionToken: CustomTabsSessionToken? = null
) {

    companion object {
        const val EXTRA_NAVIGATION_BAR_COLOR = "androidx.browser.customtabs.extra.NAVIGATION_BAR_COLOR"
        const val EXTRA_ADDITIONAL_TRUSTED_ORIGINS = "android.support.customtabs.extra.ADDITIONAL_TRUSTED_ORIGINS"
    }
}

data class CustomTabActionButtonConfig(
    val description: String,
    val icon: Bitmap,
    val pendingIntent: PendingIntent,
    val id: Int = CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID,
    val tint: Boolean = false
)

data class CustomTabMenuItem(
    val name: String,
    val pendingIntent: PendingIntent
)
