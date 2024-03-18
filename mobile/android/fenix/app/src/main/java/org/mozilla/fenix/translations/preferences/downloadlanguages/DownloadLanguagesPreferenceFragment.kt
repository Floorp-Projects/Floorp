/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.downloadlanguages

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.Fragment
import androidx.navigation.findNavController
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A fragment displaying Download Languages screen.
 */
class DownloadLanguagesPreferenceFragment : Fragment() {
    private val downloadLanguagesFeature =
        ViewBoundFeatureWrapper<DownloadLanguagesFeature>()
    private var isDataSaverEnabledAndWifiDisabled = false

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.download_languages_toolbar_title_preference))
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setContent {
            FirefoxTheme {
                DownloadLanguagesPreference(
                    downloadLanguageItemPreferences = getLanguageListPreference(),
                    onItemClick = { downloadLanguageItemPreference ->
                        if (downloadLanguageItemPreference.state.status ==
                            DownloadLanguageItemStatusPreference.Downloaded ||
                            shouldShowPrefDownloadLanguageFileDialog(
                                downloadLanguageItemPreference,
                            )
                        ) {
                            downloadLanguageItemPreference.languageModel.language?.localizedDisplayName?.let {
                                findNavController().navigate(
                                    DownloadLanguagesPreferenceFragmentDirections
                                        .actionDownloadLanguagesPreferenceToDownloadLanguagesDialogPreference(
                                            downloadLanguageItemPreference.state,
                                            it,
                                            downloadLanguageItemPreference.languageModel.size,
                                        ),
                                )
                            }
                        }
                    },
                )
            }
        }
    }

    private fun shouldShowPrefDownloadLanguageFileDialog(
        downloadLanguageItemPreference: DownloadLanguageItemPreference,
    ) =
        (
            downloadLanguageItemPreference.state.status == DownloadLanguageItemStatusPreference.NotDownloaded &&
                isDataSaverEnabledAndWifiDisabled &&
                !requireContext().settings().ignoreTranslationsDataSaverWarning
            )

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        downloadLanguagesFeature.set(
            feature = DownloadLanguagesFeature(
                context = requireContext(),
                wifiConnectionMonitor = requireContext().components.wifiConnectionMonitor,
                onDataSaverAndWifiChanged = {
                    isDataSaverEnabledAndWifiDisabled = it
                },
            ),
            owner = this,
            view = view,
        )
    }
}
