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
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.ModelManagementOptions
import mozilla.components.concept.engine.translate.ModelOperation
import mozilla.components.concept.engine.translate.ModelState
import mozilla.components.concept.engine.translate.OperationLevel
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme
import java.util.Locale

/**
 * A fragment dialog displays a delete or download language.
 */
class LanguageDialogPreferenceFragment : DialogFragment() {
    private val args by navArgs<LanguageDialogPreferenceFragmentArgs>()
    private val browserStore: BrowserStore by lazy { requireComponents.core.store }

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
        if (args.modelState == ModelState.DOWNLOADED) {
            setPrefDeleteLanguageFileDialog(view)
        } else {
            if (args.modelState == ModelState.NOT_DOWNLOADED) {
                setDownloadLanguageFileDialog(view)
            }
        }

        return view
    }

    private fun setPrefDeleteLanguageFileDialog(composeView: ComposeView) {
        composeView.apply {
            setContent {
                FirefoxTheme {
                    val languageModels = browserStore.observeAsComposableState { state ->
                        state.translationEngine.languageModels
                    }.value?.toMutableList()

                    DeleteLanguageFileDialog(
                        language = args.languageDisplayName,
                        isAllLanguagesItemType =
                        args.itemType ==
                            DownloadLanguageItemTypePreference.AllLanguages,
                        fileSize = args.modelSize,
                        onConfirmDelete = {
                            if (args.itemType == DownloadLanguageItemTypePreference.AllLanguages) {
                                languageModels?.let {
                                    val downloadedItems = it.filter { languageModel ->
                                        languageModel.status == ModelState.DOWNLOADED
                                    }

                                    for (downloadedItem in downloadedItems) {
                                        if (!downloadedItem.language?.code.equals(
                                                Locale.ENGLISH.language,
                                            )
                                        ) {
                                            deleteOrDownloadModel(
                                                modelOperation = ModelOperation.DELETE,
                                                languageToManage = downloadedItem.language?.code,
                                            )
                                        }
                                    }
                                }
                            } else {
                                deleteOrDownloadModel(
                                    modelOperation = ModelOperation.DELETE,
                                    languageToManage = args.languageCode,
                                )
                            }
                            findNavController().popBackStack()
                        },
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
                    val languageModels = browserStore.observeAsComposableState { state ->
                        state.translationEngine.languageModels
                    }.value?.toMutableList()

                    DownloadLanguageFileDialog(
                        downloadLanguageDialogType = if (args.itemType ==
                            DownloadLanguageItemTypePreference.AllLanguages
                        ) {
                            DownloadLanguageFileDialogType.AllLanguages
                        } else {
                            DownloadLanguageFileDialogType.Default
                        },
                        fileSize = args.modelSize,
                        isCheckBoxEnabled = checkBoxEnabled,
                        onSavingModeStateChange = { checkBoxEnabled = it },
                        onConfirmDownload = {
                            requireContext().settings().ignoreTranslationsDataSaverWarning =
                                checkBoxEnabled

                            if (args.itemType == DownloadLanguageItemTypePreference.AllLanguages) {
                                languageModels?.let {
                                    val downloadedItems = it.filter { languageModel ->
                                        languageModel.status == ModelState.NOT_DOWNLOADED
                                    }
                                    for (downloadedItem in downloadedItems) {
                                        deleteOrDownloadModel(
                                            modelOperation = ModelOperation.DOWNLOAD,
                                            languageToManage = downloadedItem.language?.code,
                                        )
                                    }
                                }
                            } else {
                                deleteOrDownloadModel(
                                    modelOperation = ModelOperation.DOWNLOAD,
                                    languageToManage = args.languageCode,
                                )
                            }

                            findNavController().popBackStack()
                        },
                        onCancel = { findNavController().popBackStack() },
                    )
                }
            }
        }
    }

    private fun deleteOrDownloadModel(modelOperation: ModelOperation, languageToManage: String?) {
        val options = ModelManagementOptions(
            languageToManage = languageToManage,
            operation = modelOperation,
            operationLevel = OperationLevel.LANGUAGE,
        )
        browserStore.dispatch(
            TranslationsAction.ManageLanguageModelsAction(
                options = options,
            ),
        )
    }
}
