/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

import android.app.Activity
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.locale.LocaleUseCases
import org.mozilla.focus.locale.Locales
import org.mozilla.focus.settings.InstalledSearchEnginesSettingsFragment
import org.mozilla.gecko.util.ThreadUtils.runOnUiThread
import java.util.Locale

class LanguageMiddleware(val activity: Activity, private val localeUseCase: LocaleUseCases) :
    Middleware<LanguageScreenState, LanguageScreenAction> {

    private val storage by lazy {
        LanguageStorage(context = activity)
    }

    override fun invoke(
        context: MiddlewareContext<LanguageScreenState, LanguageScreenAction>,
        next: (LanguageScreenAction) -> Unit,
        action: LanguageScreenAction
    ) {
        when (action) {
            is LanguageScreenAction.Select -> {
                storage.saveCurrentLanguageInSharePref(action.selectedLanguage.tag)
                setCurrentLanguage(action.selectedLanguage.tag)
                next(action)
            }
            is LanguageScreenAction.InitLanguages -> {
                /**
                 * The initial LanguageScreenState when the user enters first in the screen
                 */
                context.dispatch(
                    LanguageScreenAction.UpdateLanguages(
                        storage.getLanguages(),
                        storage.getSelectedLanguageTag()
                    )
                )
            }
            else -> {
                next(action)
            }
        }
    }

    /**
     * It changes the system defined locale to the indicated Language .
     * It recreates the current activity for changes to take effect.
     *
     * @param languageTag selected Language Tag that comes from Language object
     */
    private fun setCurrentLanguage(languageTag: String) {
        InstalledSearchEnginesSettingsFragment.languageChanged = true
        val locale: Locale?
        if (languageTag == LanguageStorage.LOCALE_SYSTEM_DEFAULT) {
            LocaleManager.resetToSystemDefault(activity, localeUseCase)
        } else {
            locale = Locales.parseLocaleCode(languageTag)
            LocaleManager.setNewLocale(activity, localeUseCase, locale)
        }
        runOnUiThread { activity.recreate() }
    }
}
