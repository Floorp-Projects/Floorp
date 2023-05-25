/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.os.Build
import android.provider.Settings
import android.view.ContextThemeWrapper
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityManager
import androidx.annotation.StringRes
import mozilla.components.support.locale.LocaleManager
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.settings.advanced.getSelectedLocale
import java.lang.String.format
import java.util.Locale

/**
 * Get the BrowserApplication object from a context.
 */
val Context.application: FenixApplication
    get() = applicationContext as FenixApplication

/**
 * Get the requireComponents of this application.
 */
val Context.components: Components
    get() = application.components

/**
 * Helper function to get the MetricController off of context.
 */
val Context.metrics: MetricController
    get() = this.components.analytics.metrics

fun Context.asActivity() = (this as? ContextThemeWrapper)?.baseContext as? Activity
    ?: this as? Activity

fun Context.getPreferenceKey(@StringRes resourceId: Int): String =
    resources.getString(resourceId)

/**
 * Gets the Root View with an activity context
 *
 * @return ViewGroup? if it is able to get a root view from the context
 */
fun Context.getRootView(): View? =
    asActivity()?.window?.decorView?.findViewById<View>(android.R.id.content) as? ViewGroup

fun Context.settings() = components.settings

/**
 * Used to catch IllegalArgumentException that is thrown when
 * a string's placeholder is incorrectly formatted in a translation
 *
 * @return the formatted string in locale language or English as a fallback
 */
fun Context.getStringWithArgSafe(@StringRes resId: Int, formatArg: String): String {
    return try {
        format(getString(resId), formatArg)
    } catch (e: IllegalArgumentException) {
        // fallback to <en> string
        logDebug(
            "L10n",
            "String: " + resources.getResourceEntryName(resId) +
                " not properly formatted in: " + LocaleManager.getSelectedLocale(this).language,
        )
        val config = resources.configuration
        config.setLocale(Locale("en"))
        val localizedContext: Context = this.createConfigurationContext(config)
        return format(localizedContext.getString(resId), formatArg)
    }
}

/**
 * Used to obtain a reference to an AccessibilityManager
 * @return accessibilityManager
 */
val Context.accessibilityManager: AccessibilityManager get() =
    getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager

/**
 * Used to navigate to system notifications settings for app.
 *
 * @param onError Invoked when the activity described by the intent is not present on the device.
 */
fun Context.navigateToNotificationsSettings(
    onError: () -> Unit,
) {
    val intent = Intent()
    intent.let {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            it.action = Settings.ACTION_APP_NOTIFICATION_SETTINGS
            it.putExtra(Settings.EXTRA_APP_PACKAGE, this.packageName)
        } else {
            it.action = "android.settings.APP_NOTIFICATION_SETTINGS"
            it.putExtra("app_package", this.packageName)
            it.putExtra("app_uid", this.applicationInfo.uid)
        }
    }
    startExternalActivitySafe(intent, onError)
}

/**
 * Checks for the presence of an activity before starting it. In case it's not present,
 * [onActivityNotPresent] is invoked, preventing ActivityNotFoundException from being thrown.
 * This is useful when navigating to external activities like device permission settings,
 * notification settings, default app settings, etc.
 *
 * @param intent The Intent of the activity to resolve and start.
 * @param onActivityNotPresent Invoked when the activity to handle the intent is not present.
 */
inline fun Context.startExternalActivitySafe(intent: Intent, onActivityNotPresent: () -> Unit) {
    if (intent.resolveActivity(packageManager) != null) {
        startActivity(intent)
    } else {
        onActivityNotPresent()
    }
}

/**
 * Helper function used to determine if the user's device is set to dark mode.
 *
 * @return true if the system is considered to be in dark theme.
 */
fun Context.isSystemInDarkTheme(): Boolean =
    resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK == Configuration.UI_MODE_NIGHT_YES
