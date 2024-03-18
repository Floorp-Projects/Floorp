/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.PendingIntent
import android.content.Intent
import android.content.res.Resources
import android.graphics.Bitmap
import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.browser.customtabs.CustomTabColorSchemeParams
import androidx.browser.customtabs.CustomTabsIntent
import androidx.browser.customtabs.CustomTabsIntent.ColorScheme
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_COLOR_SCHEME
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_COLOR_SCHEME_PARAMS
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_ENABLE_URLBAR_HIDING
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_MENU_ITEMS
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_NAVIGATION_BAR_COLOR
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_NAVIGATION_BAR_DIVIDER_COLOR
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_SESSION
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_SHARE_STATE
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_TINT_ACTION_BUTTON
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_TOOLBAR_COLOR
import androidx.browser.customtabs.CustomTabsIntent.KEY_DESCRIPTION
import androidx.browser.customtabs.CustomTabsIntent.KEY_ICON
import androidx.browser.customtabs.CustomTabsIntent.KEY_ID
import androidx.browser.customtabs.CustomTabsIntent.KEY_MENU_ITEM_TITLE
import androidx.browser.customtabs.CustomTabsIntent.KEY_PENDING_INTENT
import androidx.browser.customtabs.CustomTabsIntent.NO_TITLE
import androidx.browser.customtabs.CustomTabsIntent.SHARE_STATE_DEFAULT
import androidx.browser.customtabs.CustomTabsIntent.SHARE_STATE_ON
import androidx.browser.customtabs.CustomTabsIntent.SHOW_PAGE_TITLE
import androidx.browser.customtabs.CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID
import androidx.browser.customtabs.CustomTabsSessionToken
import androidx.browser.customtabs.TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY
import mozilla.components.browser.state.state.ColorSchemeParams
import mozilla.components.browser.state.state.ColorSchemes
import mozilla.components.browser.state.state.CustomTabActionButtonConfig
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeBundle
import mozilla.components.support.utils.toSafeIntent
import kotlin.math.max

/**
 * Checks if the provided intent is a custom tab intent.
 *
 * @param intent the intent to check.
 * @return true if the intent is a custom tab intent, otherwise false.
 */
fun isCustomTabIntent(intent: Intent) = isCustomTabIntent(intent.toSafeIntent())

/**
 * Checks if the provided intent is a custom tab intent.
 *
 * @param safeIntent the intent to check, wrapped as a SafeIntent.
 * @return true if the intent is a custom tab intent, otherwise false.
 */
fun isCustomTabIntent(safeIntent: SafeIntent) = safeIntent.hasExtra(EXTRA_SESSION)

/**
 * Checks if the provided intent is a trusted web activity intent.
 *
 * @param intent the intent to check.
 * @return true if the intent is a trusted web activity intent, otherwise false.
 */
fun isTrustedWebActivityIntent(intent: Intent) = isTrustedWebActivityIntent(intent.toSafeIntent())

/**
 * Checks if the provided intent is a trusted web activity intent.
 *
 * @param safeIntent the intent to check, wrapped as a SafeIntent.
 * @return true if the intent is a trusted web activity intent, otherwise false.
 */
fun isTrustedWebActivityIntent(safeIntent: SafeIntent) = isCustomTabIntent(safeIntent) &&
    safeIntent.getBooleanExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false)

/**
 * Creates a [CustomTabConfig] instance based on the provided [Intent].
 *
 * @param intent The [Intent] wrapped as a [SafeIntent], which is processed to extract configuration data.
 * @param resources Optional [Resources] to verify that only icons of a max size are provided.
 *
 * @return the configured [CustomTabConfig].
 */
fun createCustomTabConfigFromIntent(intent: Intent, resources: Resources?): CustomTabConfig {
    val safeIntent = intent.toSafeIntent()

    return CustomTabConfig(
        colorScheme = safeIntent.getColorExtra(EXTRA_COLOR_SCHEME),
        colorSchemes = getColorSchemes(safeIntent),
        closeButtonIcon = getCloseButtonIcon(safeIntent, resources),
        enableUrlbarHiding = safeIntent.getBooleanExtra(EXTRA_ENABLE_URLBAR_HIDING, false),
        actionButtonConfig = getActionButtonConfig(safeIntent),
        showShareMenuItem = (safeIntent.getIntExtra(EXTRA_SHARE_STATE, SHARE_STATE_DEFAULT) == SHARE_STATE_ON),
        menuItems = getMenuItems(safeIntent),
        exitAnimations = safeIntent.getBundleExtra(EXTRA_EXIT_ANIMATION_BUNDLE)?.unsafe,
        titleVisible = safeIntent.getIntExtra(EXTRA_TITLE_VISIBILITY_STATE, NO_TITLE) == SHOW_PAGE_TITLE,
        sessionToken = if (intent.extras != null) {
            // getSessionTokenFromIntent throws if extras is null
            CustomTabsSessionToken.getSessionTokenFromIntent(intent)
        } else {
            null
        },
        externalAppType = ExternalAppType.CUSTOM_TAB,
    )
}

@ColorInt
private fun SafeIntent.getColorExtra(name: String): Int? =
    if (hasExtra(name)) getIntExtra(name, 0) else null

private fun getCloseButtonIcon(intent: SafeIntent, resources: Resources?): Bitmap? {
    val icon = try {
        intent.getParcelableExtra(EXTRA_CLOSE_BUTTON_ICON, Bitmap::class.java)
    } catch (e: ClassCastException) {
        null
    }
    val maxSize = resources?.getDimension(R.dimen.mozac_feature_customtabs_max_close_button_size) ?: Float.MAX_VALUE

    return if (icon != null && max(icon.width, icon.height) <= maxSize) {
        icon
    } else {
        null
    }
}

private fun getColorSchemes(safeIntent: SafeIntent): ColorSchemes? {
    val defaultColorSchemeParams = getDefaultSchemeColorParams(safeIntent)
    val lightColorSchemeParams = getLightColorSchemeParams(safeIntent)
    val darkColorSchemeParams = getDarkColorSchemeParams(safeIntent)

    return if (allNull(defaultColorSchemeParams, lightColorSchemeParams, darkColorSchemeParams)) {
        null
    } else {
        ColorSchemes(
            defaultColorSchemeParams = defaultColorSchemeParams,
            lightColorSchemeParams = lightColorSchemeParams,
            darkColorSchemeParams = darkColorSchemeParams,
        )
    }
}

/**
 * Processes the given [SafeIntent] to extract possible default [CustomTabColorSchemeParams]
 * properties.
 *
 * @param safeIntent the [SafeIntent] to process.
 *
 * @return the derived [ColorSchemeParams] or null if the [SafeIntent] had no default
 * [CustomTabColorSchemeParams] properties.
 *
 * @see [CustomTabsIntent.Builder.setDefaultColorSchemeParams].
 */
private fun getDefaultSchemeColorParams(safeIntent: SafeIntent): ColorSchemeParams? {
    val toolbarColor = safeIntent.getColorExtra(EXTRA_TOOLBAR_COLOR)
    val secondaryToolbarColor = safeIntent.getColorExtra(EXTRA_SECONDARY_TOOLBAR_COLOR)
    val navigationBarColor = safeIntent.getColorExtra(EXTRA_NAVIGATION_BAR_COLOR)
    val navigationBarDividerColor = safeIntent.getColorExtra(EXTRA_NAVIGATION_BAR_DIVIDER_COLOR)

    return if (allNull(
            toolbarColor,
            secondaryToolbarColor,
            navigationBarColor,
            navigationBarDividerColor,
        )
    ) {
        null
    } else {
        ColorSchemeParams(
            toolbarColor = toolbarColor,
            secondaryToolbarColor = secondaryToolbarColor,
            navigationBarColor = navigationBarColor,
            navigationBarDividerColor = navigationBarDividerColor,
        )
    }
}

private fun getLightColorSchemeParams(safeIntent: SafeIntent) =
    getColorSchemeParams(safeIntent, CustomTabsIntent.COLOR_SCHEME_LIGHT)

private fun getDarkColorSchemeParams(safeIntent: SafeIntent) =
    getColorSchemeParams(safeIntent, CustomTabsIntent.COLOR_SCHEME_DARK)

/**
 * Processes the given [SafeIntent] to extract possible [CustomTabColorSchemeParams] properties for
 * the given [colorScheme].
 *
 * @param safeIntent The [SafeIntent] to process.
 * @param colorScheme The [ColorScheme] to get the [ColorSchemeParams] for.
 *
 * @return the derived [ColorSchemeParams] for the given [ColorScheme], or null if the [SafeIntent]
 * had no [CustomTabColorSchemeParams] properties for the [ColorScheme].
 *
 * @see [CustomTabsIntent.Builder.setColorSchemeParams].
 */
private fun getColorSchemeParams(safeIntent: SafeIntent, @ColorScheme colorScheme: Int): ColorSchemeParams? {
    val bundle = safeIntent.getColorSchemeParamsBundle()?.get(colorScheme)

    val toolbarColor = bundle?.getNullableSafeValue(EXTRA_TOOLBAR_COLOR)
    val secondaryToolbarColor = bundle?.getNullableSafeValue(EXTRA_SECONDARY_TOOLBAR_COLOR)
    val navigationBarColor = bundle?.getNullableSafeValue(EXTRA_NAVIGATION_BAR_COLOR)
    val navigationBarDividerColor = bundle?.getNullableSafeValue(EXTRA_NAVIGATION_BAR_DIVIDER_COLOR)

    return if (allNull(toolbarColor, secondaryToolbarColor, navigationBarColor, navigationBarDividerColor)) {
        null
    } else {
        ColorSchemeParams(
            toolbarColor = toolbarColor,
            secondaryToolbarColor = secondaryToolbarColor,
            navigationBarColor = navigationBarColor,
            navigationBarDividerColor = navigationBarDividerColor,
        )
    }
}

private fun <T> allNull(vararg value: T?) = value.toList().all { it == null }

@VisibleForTesting
internal fun SafeIntent.getColorSchemeParamsBundle() = extras?.let {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
        @Suppress("DEPRECATION")
        it.getSparseParcelableArray(EXTRA_COLOR_SCHEME_PARAMS)
    } else {
        it.getSparseParcelableArray(EXTRA_COLOR_SCHEME_PARAMS, Bundle::class.java)
    }
}

private fun Bundle.getNullableSafeValue(key: String) =
    if (containsKey(key)) toSafeBundle().getInt(key) else null

private fun getActionButtonConfig(intent: SafeIntent): CustomTabActionButtonConfig? {
    val actionButtonBundle = intent.getBundleExtra(EXTRA_ACTION_BUTTON_BUNDLE) ?: return null
    val description = actionButtonBundle.getString(KEY_DESCRIPTION)
    val icon = actionButtonBundle.getParcelable(KEY_ICON, Bitmap::class.java)
    val pendingIntent = actionButtonBundle.getParcelable(KEY_PENDING_INTENT, PendingIntent::class.java)
    val id = actionButtonBundle.getInt(KEY_ID, TOOLBAR_ACTION_BUTTON_ID)
    val tint = intent.getBooleanExtra(EXTRA_TINT_ACTION_BUTTON, false)

    return if (description != null && icon != null && pendingIntent != null) {
        CustomTabActionButtonConfig(
            id = id,
            description = description,
            icon = icon,
            pendingIntent = pendingIntent,
            tint = tint,
        )
    } else {
        null
    }
}

private fun getMenuItems(intent: SafeIntent): List<CustomTabMenuItem> =
    intent.getParcelableArrayListExtra(EXTRA_MENU_ITEMS, Parcelable::class.java).orEmpty()
        .mapNotNull { menuItemBundle ->
            val bundle = (menuItemBundle as? Bundle)?.toSafeBundle()
            val name = bundle?.getString(KEY_MENU_ITEM_TITLE)
            val pendingIntent = bundle?.getParcelable(KEY_PENDING_INTENT, PendingIntent::class.java)

            if (name != null && pendingIntent != null) {
                CustomTabMenuItem(
                    name = name,
                    pendingIntent = pendingIntent,
                )
            } else {
                null
            }
        }
