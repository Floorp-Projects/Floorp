/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.app.Activity
import android.content.Context
import android.view.ContextThemeWrapper
import android.view.View
import android.view.ViewGroup
import androidx.annotation.StringRes
import androidx.fragment.app.FragmentActivity
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.support.locale.LocaleManager
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.settings.advanced.getSelectedLocale
import org.mozilla.fenix.utils.Settings
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

/**
 * Helper function to get the SearchEngineManager off of context.
 */
val Context.searchEngineManager: SearchEngineManager
    get() = this.components.search.searchEngineManager

fun Context.asActivity() = (this as? ContextThemeWrapper)?.baseContext as? Activity
    ?: this as? Activity

fun Context.asFragmentActivity() = (this as? ContextThemeWrapper)?.baseContext as? FragmentActivity
    ?: this as? FragmentActivity

fun Context.getPreferenceKey(@StringRes resourceId: Int): String =
    resources.getString(resourceId)

/**
 * Gets the Root View with an activity context
 *
 * @return ViewGroup? if it is able to get a root view from the context
 */
fun Context.getRootView(): View? =
    asActivity()?.window?.decorView?.findViewById<View>(android.R.id.content) as? ViewGroup

fun Context.settings(isCrashReportEnabledInBuild: Boolean = BuildConfig.CRASH_REPORTING && Config.channel.isReleased) =
    Settings.getInstance(this, isCrashReportEnabledInBuild)

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
                " not properly formatted in: " + LocaleManager.getSelectedLocale(this).language
        )
        val config = resources.configuration
        config.setLocale(Locale("en"))
        val localizedContext: Context = this.createConfigurationContext(config)
        return format(localizedContext.getString(resId), formatArg)
    }
}
