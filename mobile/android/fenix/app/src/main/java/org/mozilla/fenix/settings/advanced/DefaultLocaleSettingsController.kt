/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.app.Activity
import android.content.Context
import android.os.Build
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.locale.LocaleUseCases
import org.mozilla.fenix.nimbus.FxNimbus
import java.util.Locale

interface LocaleSettingsController {
    fun handleLocaleSelected(locale: Locale)
    fun handleSearchQueryTyped(query: String)
    fun handleDefaultLocaleSelected()
}

class DefaultLocaleSettingsController(
    private val activity: Activity,
    private val localeSettingsStore: LocaleSettingsStore,
    private val browserStore: BrowserStore,
    private val localeUseCase: LocaleUseCases,
) : LocaleSettingsController {

    override fun handleLocaleSelected(locale: Locale) {
        if (localeSettingsStore.state.selectedLocale == locale &&
            !LocaleManager.isDefaultLocaleSelected(activity)
        ) {
            return
        }
        localeSettingsStore.dispatch(LocaleSettingsAction.Select(locale))
        browserStore.dispatch(SearchAction.RefreshSearchEnginesAction)
        LocaleManager.setNewLocale(activity, localeUseCase, locale)
        LocaleManager.updateBaseConfiguration(activity, locale)

        // Invalidate cached values to use the new locale
        FxNimbus.features.nimbusValidation.withCachedValue(null)
        activity.recreate()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            activity.overrideActivityTransition(Activity.OVERRIDE_TRANSITION_OPEN, 0, 0)
        } else {
            @Suppress("DEPRECATION")
            activity.overridePendingTransition(0, 0)
        }
    }

    override fun handleDefaultLocaleSelected() {
        if (LocaleManager.isDefaultLocaleSelected(activity)) {
            return
        }
        localeSettingsStore.dispatch(LocaleSettingsAction.Select(localeSettingsStore.state.localeList[0]))
        browserStore.dispatch(SearchAction.RefreshSearchEnginesAction)
        LocaleManager.resetToSystemDefault(activity, localeUseCase)
        LocaleManager.updateBaseConfiguration(activity, localeSettingsStore.state.localeList[0])

        // Invalidate cached values to use the default locale
        FxNimbus.features.nimbusValidation.withCachedValue(null)
        activity.recreate()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            activity.overrideActivityTransition(Activity.OVERRIDE_TRANSITION_OPEN, 0, 0)
        } else {
            @Suppress("DEPRECATION")
            activity.overridePendingTransition(0, 0)
        }
    }

    override fun handleSearchQueryTyped(query: String) {
        localeSettingsStore.dispatch(LocaleSettingsAction.Search(query))
    }

    /**
     * Update the locale for the configuration of the app context's resources
     */
    @Suppress("Deprecation")
    fun LocaleManager.updateBaseConfiguration(context: Context, locale: Locale) {
        val resources = context.applicationContext.resources
        val config = resources.configuration
        config.setLocale(locale)
        config.setLayoutDirection(locale)
        resources.updateConfiguration(config, resources.displayMetrics)
    }
}
