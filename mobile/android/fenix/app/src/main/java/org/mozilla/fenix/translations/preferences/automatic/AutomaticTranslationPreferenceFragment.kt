/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.automatic

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.LanguageSetting
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.concept.engine.translate.findLanguage
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A fragment displaying the Firefox Automatic Translation list screen.
 */
class AutomaticTranslationPreferenceFragment : Fragment() {
    private val browserStore: BrowserStore by lazy { requireComponents.core.store }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.automatic_translation_toolbar_title_preference))
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setContent {
            FirefoxTheme {
                val languageSettings = browserStore.observeAsComposableState { state ->
                    state.translationEngine.languageSettings
                }.value
                val translationSupport = browserStore.observeAsComposableState { state ->
                    state.translationEngine.supportedLanguages
                }.value
                val engineError = browserStore.observeAsComposableState { state ->
                    state.translationEngine.engineError
                }.value
                val couldNotLoadLanguagesError =
                    engineError as? TranslationError.CouldNotLoadLanguagesError
                val couldNotLoadLanguageSettingsError =
                    engineError as? TranslationError.CouldNotLoadLanguageSettingsError

                AutomaticTranslationPreference(
                    automaticTranslationListPreferences = getAutomaticTranslationListPreferences(
                        languageSettings = languageSettings,
                        translationSupport = translationSupport,
                    ),
                    hasLanguageError = couldNotLoadLanguagesError != null ||
                        couldNotLoadLanguageSettingsError != null ||
                        languageSettings == null,
                    onItemClick = {
                        findNavController().navigate(
                            AutomaticTranslationPreferenceFragmentDirections
                                .actionAutomaticTranslationPreferenceToAutomaticTranslationOptionsPreference(
                                    it,
                                ),
                        )
                    },
                )
            }
        }
    }

    private fun getAutomaticTranslationListPreferences(
        languageSettings: Map<String, LanguageSetting>? = null,
        translationSupport: TranslationSupport? = null,
    ): List<AutomaticTranslationItemPreference> {
        val automaticTranslationListPreferences =
            mutableListOf<AutomaticTranslationItemPreference>()

        if (translationSupport != null && languageSettings != null) {
            languageSettings.forEach { entry ->
                translationSupport.findLanguage(entry.key)?.let {
                    automaticTranslationListPreferences.add(
                        AutomaticTranslationItemPreference(
                            language = it,
                            automaticTranslationOptionPreference = getAutomaticTranslationOptionPreference(
                                entry.value,
                            ),
                        ),
                    )
                }
            }
        }
        return automaticTranslationListPreferences
    }
}
