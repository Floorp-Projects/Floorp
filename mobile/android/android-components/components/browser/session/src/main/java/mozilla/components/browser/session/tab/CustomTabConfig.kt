/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.tab

import android.app.PendingIntent
import android.graphics.Bitmap
import android.os.Bundle
import android.os.Parcelable
import android.util.DisplayMetrics
import androidx.annotation.ColorInt
import androidx.browser.customtabs.CustomTabsIntent
import mozilla.components.support.utils.SafeBundle
import mozilla.components.support.utils.SafeIntent
import java.util.ArrayList
import java.util.UUID

/**
 * Holds configuration data for a Custom Tab.
 *
 * @property toolbarColor Background color for the toolbar.
 * @property closeButtonIcon Custom icon of the back button on the toolbar.
 * @property enableUrlbarHiding Enables the toolbar to hide as the user scrolls down on the page.
 * @property actionButtonConfig Custom action button on the toolbar.
 * @property showShareMenuItem Specifies whether a default share button will be shown in the menu.
 * @property menuItems Custom overflow menu items.
 * @property exitAnimations Bundle containing custom exit animations for the tab.
 * @property navigationBarColor Background color for the navigation bar.
 * @property titleVisible Whether the title should be shown in the custom tab.
 */
data class CustomTabConfig(
    val id: String,
    @ColorInt val toolbarColor: Int?,
    val closeButtonIcon: Bitmap?,
    val enableUrlbarHiding: Boolean,
    val actionButtonConfig: CustomTabActionButtonConfig?,
    val showShareMenuItem: Boolean,
    val menuItems: List<CustomTabMenuItem> = emptyList(),
    val exitAnimations: Bundle? = null,
    @ColorInt val navigationBarColor: Int? = null,
    val titleVisible: Boolean = false
) {
    inline val disableUrlbarHiding
        get() = !enableUrlbarHiding

    val options: List<String> by lazy { generateOptions() }

    @Suppress("ComplexMethod")
    private fun generateOptions(): List<String> {
        val options = mutableListOf<String>()

        if (toolbarColor != null) options.add(TOOLBAR_COLOR_OPTION)
        if (closeButtonIcon != null) options.add(CLOSE_BUTTON_OPTION)
        if (enableUrlbarHiding) options.add(DISABLE_URLBAR_HIDING_OPTION)
        if (actionButtonConfig != null) options.add(ACTION_BUTTON_OPTION)
        if (showShareMenuItem) options.add(SHARE_MENU_ITEM_OPTION)
        if (menuItems.isNotEmpty()) options.add(CUSTOMIZED_MENU_OPTION)
        if (actionButtonConfig?.tint == true) options.add(ACTION_BUTTON_TINT_OPTION)

        if (exitAnimations != null) options.add(EXIT_ANIMATION_OPTION)
        if (titleVisible) options.add(PAGE_TITLE_OPTION)

        return options
    }

    companion object {
        internal const val TOOLBAR_COLOR_OPTION = "hasToolbarColor"
        internal const val CLOSE_BUTTON_OPTION = "hasCloseButton"
        internal const val DISABLE_URLBAR_HIDING_OPTION = "disablesUrlbarHiding"
        internal const val ACTION_BUTTON_OPTION = "hasActionButton"
        internal const val SHARE_MENU_ITEM_OPTION = "hasShareItem"
        internal const val CUSTOMIZED_MENU_OPTION = "hasCustomizedMenu"
        internal const val ACTION_BUTTON_TINT_OPTION = "hasActionButtonTint"
        internal const val BOTTOM_TOOLBAR_OPTION = "hasBottomToolbar"
        internal const val EXIT_ANIMATION_OPTION = "hasExitAnimation"
        internal const val PAGE_TITLE_OPTION = "hasPageTitle"
        private const val MAX_CLOSE_BUTTON_SIZE_DP = 24

        const val EXTRA_NAVIGATION_BAR_COLOR = "androidx.browser.customtabs.extra.NAVIGATION_BAR_COLOR"
        private val EXTRA_SESSION = StringBuilder("support.customtabs.extra.SESSION").toExtra()
        private val EXTRA_TOOLBAR_COLOR = StringBuilder("support.customtabs.extra.TOOLBAR_COLOR").toExtra()
        private val EXTRA_CLOSE_BUTTON_ICON = StringBuilder("support.customtabs.extra.CLOSE_BUTTON_ICON").toExtra()
        private val EXTRA_ACTION_BUTTON_BUNDLE =
            StringBuilder("support.customtabs.extra.ACTION_BUTTON_BUNDLE").toExtra()
        private val KEY_DESCRIPTION = StringBuilder("support.customtabs.customaction.DESCRIPTION").toExtra()
        private val KEY_PENDING_INTENT = StringBuilder("support.customtabs.customaction.PENDING_INTENT").toExtra()
        private val EXTRA_ENABLE_URLBAR_HIDING =
            StringBuilder("support.customtabs.extra.ENABLE_URLBAR_HIDING").toExtra()
        private val EXTRA_DEFAULT_SHARE_MENU_ITEM = StringBuilder("support.customtabs.extra.SHARE_MENU_ITEM").toExtra()
        private val EXTRA_MENU_ITEMS = StringBuilder("support.customtabs.extra.MENU_ITEMS").toExtra()
        private val KEY_MENU_ITEM_TITLE = StringBuilder("support.customtabs.customaction.MENU_ITEM_TITLE").toExtra()

        /**
         * TODO remove when fixed: https://github.com/mozilla-mobile/android-components/issues/1884
         */
        private fun StringBuilder.toExtra() = insert(0, "android.").toString()

        /**
         * Checks if the provided intent is a custom tab intent.
         *
         * @param intent the intent to check, wrapped as a SafeIntent.
         * @return true if the intent is a custom tab intent, otherwise false.
         */
        @Deprecated("Use isCustomTabIntent in feature-customtabs",
            ReplaceWith("isCustomTabIntent(intent.unsafe)", "mozilla.components.feature.customtabs.isCustomTabIntent"))
        fun isCustomTabIntent(intent: SafeIntent): Boolean {
            return intent.hasExtra(EXTRA_SESSION)
        }

        /**
         * Creates a CustomTabConfig instance based on the provided intent.
         *
         * @param intent the intent, wrapped as a SafeIntent, which is processed
         * to extract configuration data.
         * @param displayMetrics needed in-order to verify that icons of a max size are only provided.
         * @return the CustomTabConfig instance.
         */
        @Deprecated("Use createCustomTabConfigFromIntent in feature-customtabs",
            ReplaceWith(
                "createCustomTabConfigFromIntent(intent.unsafe)",
                "mozilla.components.feature.customtabs.createCustomTabConfigFromIntent"
            ))
        @Suppress("ComplexMethod")
        fun createFromIntent(intent: SafeIntent, displayMetrics: DisplayMetrics? = null): CustomTabConfig {
            val id = UUID.randomUUID().toString()

            val toolbarColor = if (intent.hasExtra(EXTRA_TOOLBAR_COLOR)) {
                intent.getIntExtra(EXTRA_TOOLBAR_COLOR, -1)
            } else {
                null
            }

            val closeButtonIcon = run {
                val icon = intent.getParcelableExtra(EXTRA_CLOSE_BUTTON_ICON) as? Bitmap
                val density = displayMetrics?.density ?: 1f
                if (icon != null &&
                    icon.width / density <= MAX_CLOSE_BUTTON_SIZE_DP &&
                    icon.height / density <= MAX_CLOSE_BUTTON_SIZE_DP
                ) {
                    icon
                } else {
                    null
                }
            }

            val enableUrlbarHiding = intent.getBooleanExtra(EXTRA_ENABLE_URLBAR_HIDING, true)

            val actionButtonConfig = getActionButtonConfig(intent)

            val showShareMenuItem = intent.getBooleanExtra(EXTRA_DEFAULT_SHARE_MENU_ITEM, false)

            val menuItems = getMenuItems(intent)

            // We are currently ignoring EXTRA_SECONDARY_TOOLBAR_COLOR and EXTRA_ENABLE_INSTANT_APPS
            // due to https://github.com/mozilla-mobile/focus-android/issues/629

            return CustomTabConfig(id, toolbarColor, closeButtonIcon, enableUrlbarHiding, actionButtonConfig,
                    showShareMenuItem, menuItems)
        }

        private fun getActionButtonConfig(intent: SafeIntent): CustomTabActionButtonConfig? {
            var buttonConfig: CustomTabActionButtonConfig? = null
            if (intent.hasExtra(EXTRA_ACTION_BUTTON_BUNDLE)) {
                val abBundle = intent.getBundleExtra(EXTRA_ACTION_BUTTON_BUNDLE)
                val description = abBundle?.getString(KEY_DESCRIPTION)
                val icon = abBundle?.getParcelable(CustomTabsIntent.KEY_ICON) as? Bitmap
                val pendingIntent = abBundle?.getParcelable(KEY_PENDING_INTENT) as? PendingIntent
                if (description != null && icon != null && pendingIntent != null) {
                    buttonConfig = CustomTabActionButtonConfig(description, icon, pendingIntent)
                }
            }
            return buttonConfig
        }

        private fun getMenuItems(intent: SafeIntent): List<CustomTabMenuItem> {
            if (!intent.hasExtra(EXTRA_MENU_ITEMS)) {
                return emptyList()
            }

            val menuItems = mutableListOf<CustomTabMenuItem>()
            val menuItemBundles: ArrayList<Parcelable>? = intent.getParcelableArrayListExtra(EXTRA_MENU_ITEMS)
            menuItemBundles?.forEach {
                if (it is Bundle) {
                    val bundle = SafeBundle(it)
                    val name = bundle.getString(KEY_MENU_ITEM_TITLE)
                    val pendingIntent = bundle.getParcelable(KEY_PENDING_INTENT) as? PendingIntent
                    if (name != null && pendingIntent != null) {
                        menuItems.add(CustomTabMenuItem(name, pendingIntent))
                    }
                }
            }
            return menuItems
        }
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
