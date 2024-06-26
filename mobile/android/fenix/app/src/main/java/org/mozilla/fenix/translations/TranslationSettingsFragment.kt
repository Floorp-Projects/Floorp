/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.TranslationsBrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.GleanMetrics.Translations
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A fragment displaying the Firefox Translation settings screen.
 */
class TranslationSettingsFragment : Fragment(), UserInteractionHandler {
    private val browserStore: BrowserStore by lazy { requireComponents.core.store }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.translation_settings_toolbar_title))
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setContent {
            FirefoxTheme {
                TranslationSettings(
                    translationSwitchList = getTranslationSwitchItemList(),
                    showAutomaticTranslations = FxNimbus.features.translations.value().globalLangSettingsEnabled,
                    showNeverTranslate = FxNimbus.features.translations.value().globalSiteSettingsEnabled,
                    showDownloads = FxNimbus.features.translations.value().downloadsEnabled,
                    pageSettingsError = browserStore.observeAsComposableState { state ->
                        state.selectedTab?.translationsState?.settingsError
                    }.value,
                    onAutomaticTranslationClicked = {
                        Translations.action.record(Translations.ActionExtra("global_lang_settings"))
                        findNavController().navigate(
                            TranslationSettingsFragmentDirections
                                .actionTranslationSettingsFragmentToAutomaticTranslationPreferenceFragment(),
                        )
                    },
                    onNeverTranslationClicked = {
                        Translations.action.record(Translations.ActionExtra("global_site_settings"))
                        findNavController().navigate(
                            TranslationSettingsFragmentDirections
                                .actionTranslationSettingsToNeverTranslateSitePreference(),
                        )
                    },
                    onDownloadLanguageClicked = {
                        Translations.action.record(Translations.ActionExtra("downloads"))
                        findNavController().navigate(
                            TranslationSettingsFragmentDirections
                                .actionTranslationSettingsFragmentToDownloadLanguagesPreferenceFragment(),
                        )
                    },
                )
            }
        }
    }

    /**
     * Set the switch item values.
     * The first one is based on [TranslationsBrowserState.offerTranslation].
     * The second one is [DownloadLanguageFileDialog] visibility.
     * This pop-up will appear if the switch item is unchecked, the phone is in saving mode, and
     * doesn't have a WiFi connection.
     */
    @Composable
    private fun getTranslationSwitchItemList(): MutableList<TranslationSwitchItem> {
        val offerToTranslate = browserStore.observeAsComposableState { state ->
            state.translationEngine.offerTranslation
        }.value
        val translationSwitchItems = mutableListOf<TranslationSwitchItem>()

        offerToTranslate?.let {
            translationSwitchItems.add(
                TranslationSwitchItem(
                    type = TranslationSettingsScreenOption.OfferToTranslate(
                        hasDivider = false,
                    ),
                    textLabel = stringResource(R.string.translation_settings_offer_to_translate),
                    isChecked = it,
                    isEnabled = true,
                    onStateChange = { _, checked ->
                        browserStore.dispatch(
                            TranslationsAction.UpdateGlobalOfferTranslateSettingAction(
                                offerTranslation = checked,
                            ),
                        )
                        // Ensures persistence of value
                        requireContext().settings().offerTranslation = checked
                    },
                ),
            )
        }
        var isDownloadInSavingModeChecked by remember {
            mutableStateOf(requireContext().settings().ignoreTranslationsDataSaverWarning)
        }

        translationSwitchItems.add(
            TranslationSwitchItem(
                type = TranslationSettingsScreenOption.AlwaysDownloadInSavingMode(
                    hasDivider = true,
                ),
                textLabel = stringResource(R.string.translation_settings_always_download),
                isChecked = isDownloadInSavingModeChecked,
                isEnabled = true,
                onStateChange = { _, checked ->
                    isDownloadInSavingModeChecked = checked
                    requireContext().settings().ignoreTranslationsDataSaverWarning = checked
                },
            ),
        )
        return translationSwitchItems
    }

    override fun onBackPressed(): Boolean {
        return if (findNavController().previousBackStackEntry?.destination?.id == R.id.browserFragment) {
            findNavController().navigate(
                TranslationSettingsFragmentDirections.actionTranslationSettingsFragmentToTranslationsDialogFragment(
                    translationsDialogAccessPoint = TranslationsDialogAccessPoint.TranslationsOptions,
                ),
            )
            true
        } else {
            false
        }
    }
}
