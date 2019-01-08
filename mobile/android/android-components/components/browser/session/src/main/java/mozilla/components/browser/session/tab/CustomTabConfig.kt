/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.tab

import android.app.PendingIntent
import android.graphics.Bitmap
import android.os.Bundle
import android.os.Parcelable
import android.support.annotation.ColorInt
import android.support.customtabs.CustomTabsIntent
import android.support.customtabs.CustomTabsIntent.EXTRA_TOOLBAR_COLOR
import android.support.customtabs.CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON
import android.support.customtabs.CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE
import android.support.customtabs.CustomTabsIntent.KEY_DESCRIPTION
import android.support.customtabs.CustomTabsIntent.KEY_PENDING_INTENT
import android.support.customtabs.CustomTabsIntent.EXTRA_ENABLE_URLBAR_HIDING
import android.support.customtabs.CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM
import android.support.customtabs.CustomTabsIntent.EXTRA_MENU_ITEMS
import android.support.customtabs.CustomTabsIntent.KEY_MENU_ITEM_TITLE
import android.support.customtabs.CustomTabsIntent.EXTRA_TINT_ACTION_BUTTON
import android.support.customtabs.CustomTabsIntent.EXTRA_REMOTEVIEWS
import android.support.customtabs.CustomTabsIntent.EXTRA_TOOLBAR_ITEMS

import mozilla.components.support.utils.SafeBundle
import mozilla.components.support.utils.SafeIntent
import java.util.ArrayList
import java.util.Collections
import java.util.UUID

/**
 * Holds configuration data for a Custom Tab. Use [createFromIntent] to
 * create instances.
 */
class CustomTabConfig internal constructor(
    val id: String,
    @ColorInt val toolbarColor: Int?,
    val closeButtonIcon: Bitmap?,
    val disableUrlbarHiding: Boolean,
    val actionButtonConfig: CustomTabActionButtonConfig?,
    val showShareMenuItem: Boolean,
    val menuItems: List<CustomTabMenuItem>,
    val options: List<String>
) {
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

        /**
         * Checks if the provided intent is a custom tab intent.
         *
         * @param intent the intent to check, wrapped as a SafeIntent.
         * @return true if the intent is a custom tab intent, otherwise false.
         */
        fun isCustomTabIntent(intent: SafeIntent): Boolean {
            return intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)
        }

        /**
         * Creates a CustomTabConfig instance based on the provided intent.
         *
         * @param intent the intent, wrapped as a SafeIntent, which is processed
         * to extract configuration data.
         * @return the CustomTabConfig instance.
         */
        @Suppress("ComplexMethod")
        fun createFromIntent(intent: SafeIntent): CustomTabConfig {
            val id = UUID.randomUUID().toString()

            val options = mutableListOf<String>()

            val toolbarColor = if (intent.hasExtra(EXTRA_TOOLBAR_COLOR)) {
                options.add(TOOLBAR_COLOR_OPTION)
                intent.getIntExtra(EXTRA_TOOLBAR_COLOR, -1)
            } else {
                null
            }

            val closeButtonIcon = run {
                val icon = intent.getParcelableExtra(EXTRA_CLOSE_BUTTON_ICON) as? Bitmap
                if (icon != null) {
                    options.add(CLOSE_BUTTON_OPTION)
                    icon
                } else {
                    null
                }
            }

            val disableUrlbarHiding = !intent.getBooleanExtra(EXTRA_ENABLE_URLBAR_HIDING, true)
            if (!disableUrlbarHiding) {
                options.add(DISABLE_URLBAR_HIDING_OPTION)
            }

            val actionButtonConfig = getActionButtonConfig(intent)
            if (actionButtonConfig != null) {
                options.add(ACTION_BUTTON_OPTION)
            }

            val showShareMenuItem = intent.getBooleanExtra(EXTRA_DEFAULT_SHARE_MENU_ITEM, true)
            if (showShareMenuItem) {
                options.add(SHARE_MENU_ITEM_OPTION)
            }

            val menuItems = getMenuItems(intent)
            if (menuItems.isNotEmpty()) {
                options.add(CUSTOMIZED_MENU_OPTION)
            }

            if (intent.hasExtra(EXTRA_TINT_ACTION_BUTTON)) {
                options.add(ACTION_BUTTON_TINT_OPTION)
            }

            if (intent.hasExtra(EXTRA_REMOTEVIEWS) || intent.hasExtra(EXTRA_TOOLBAR_ITEMS)) {
                options.add(BOTTOM_TOOLBAR_OPTION)
            }

            if (intent.hasExtra(CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE)) {
                options.add(EXIT_ANIMATION_OPTION)
            }

            if (intent.hasExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE)) {
                val titleVisibility = intent.getIntExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, 0)
                if (titleVisibility == CustomTabsIntent.SHOW_PAGE_TITLE) {
                    options.add(PAGE_TITLE_OPTION)
                }
            }

            // We are currently ignoring EXTRA_SECONDARY_TOOLBAR_COLOR and EXTRA_ENABLE_INSTANT_APPS
            // due to https://github.com/mozilla-mobile/focus-android/issues/629

            return CustomTabConfig(id, toolbarColor, closeButtonIcon, disableUrlbarHiding, actionButtonConfig,
                    showShareMenuItem, menuItems, Collections.unmodifiableList(options))
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

data class CustomTabActionButtonConfig(val description: String, val icon: Bitmap, val pendingIntent: PendingIntent)
data class CustomTabMenuItem(val name: String, val pendingIntent: PendingIntent)
