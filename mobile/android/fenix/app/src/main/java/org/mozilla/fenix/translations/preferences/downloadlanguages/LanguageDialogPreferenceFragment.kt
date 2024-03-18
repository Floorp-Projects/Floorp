/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.downloadlanguages

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.DialogFragment
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A fragment dialog displays a delete or download language.
 */
class LanguageDialogPreferenceFragment : DialogFragment() {
    private val args by navArgs<LanguageDialogPreferenceFragmentArgs>()

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
                setCanceledOnTouchOutside(false)
            }
        }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val view = ComposeView(requireContext())
        if (args.downloadLanguageItemStatePreference.status == DownloadLanguageItemStatusPreference.Downloaded) {
            setPrefDeleteLanguageFileDialog(view)
        } else {
            if (args.downloadLanguageItemStatePreference.status == DownloadLanguageItemStatusPreference.NotDownloaded) {
                setDownloadLanguageFileDialog(view)
            }
        }

        return view
    }

    private fun setPrefDeleteLanguageFileDialog(composeView: ComposeView) {
        composeView.apply {
            setContent {
                FirefoxTheme {
                    DeleteLanguageFileDialog(
                        language = args.languageNamePreference,
                        isAllLanguagesItemType =
                        args.downloadLanguageItemStatePreference.type ==
                            DownloadLanguageItemTypePreference.AllLanguages,
                        fileSize = args.itemFileSizePreference,
                        onConfirmDelete = { findNavController().popBackStack() },
                        onCancel = { findNavController().popBackStack() },
                    )
                }
            }
        }
    }

    private fun setDownloadLanguageFileDialog(composeView: ComposeView) {
        composeView.apply {
            setContent {
                FirefoxTheme {
                    var checkBoxEnabled by remember { mutableStateOf(false) }
                    DownloadLanguageFileDialog(
                        downloadLanguageDialogType = if (args.downloadLanguageItemStatePreference.type ==
                            DownloadLanguageItemTypePreference.AllLanguages
                        ) {
                            DownloadLanguageFileDialogType.AllLanguages
                        } else {
                            DownloadLanguageFileDialogType.Default
                        },
                        fileSize = args.itemFileSizePreference,
                        isCheckBoxEnabled = checkBoxEnabled,
                        onSavingModeStateChange = { checkBoxEnabled = it },
                        onConfirmDownload = {
                            requireContext().settings().ignoreTranslationsDataSaverWarning =
                                checkBoxEnabled
                            findNavController().popBackStack()
                        },
                        onCancel = { findNavController().popBackStack() },
                    )
                }
            }
        }
    }
}
