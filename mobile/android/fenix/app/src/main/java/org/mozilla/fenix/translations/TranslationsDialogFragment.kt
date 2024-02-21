/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.compose.ui.unit.dp
import androidx.core.os.bundleOf
import androidx.fragment.app.setFragmentResult
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.translations.preferences.downloadlanguages.DownloadLanguageFileDialog
import org.mozilla.fenix.translations.preferences.downloadlanguages.DownloadLanguageFileDialogType
import org.mozilla.fenix.translations.preferences.downloadlanguages.DownloadLanguagesFeature

/**
 * The enum is to know what bottom sheet to open.
 */
enum class TranslationsDialogAccessPoint {
    Translations, TranslationsOptions,
}

/**
 * A bottom sheet fragment displaying the Firefox Translation dialog.
 */
class TranslationsDialogFragment : BottomSheetDialogFragment() {

    private var behavior: BottomSheetBehavior<View>? = null
    private val args by navArgs<TranslationsDialogFragmentArgs>()
    private val browserStore: BrowserStore by lazy { requireComponents.core.store }
    private val translationDialogBinding = ViewBoundFeatureWrapper<TranslationsDialogBinding>()
    private val downloadLanguagesFeature =
        ViewBoundFeatureWrapper<DownloadLanguagesFeature>()
    private lateinit var translationsDialogStore: TranslationsDialogStore
    private var isTranslationInProgress: Boolean? = null
    private var isDataSaverEnabledAndWifiDisabled = false

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
                val bottomSheet = findViewById<View?>(R.id.design_bottom_sheet)
                bottomSheet?.setBackgroundResource(android.R.color.transparent)
                behavior = BottomSheetBehavior.from(bottomSheet)
                behavior?.state = BottomSheetBehavior.STATE_EXPANDED
            }
        }

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        translationsDialogStore = TranslationsDialogStore(
            TranslationsDialogState(),
            listOf(
                TranslationsDialogMiddleware(
                    browserStore = browserStore,
                    sessionId = args.sessionId,
                ),
            ),
        )
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)
        setContent {
            FirefoxTheme {
                var translationsVisibility by remember {
                    mutableStateOf(
                        args.translationsDialogAccessPoint == TranslationsDialogAccessPoint.Translations,
                    )
                }

                var translationsHeightDp by remember {
                    mutableStateOf(0.dp)
                }

                var translationsOptionsHeightDp by remember {
                    mutableStateOf(0.dp)
                }

                var translationsWidthDp by remember {
                    mutableStateOf(0.dp)
                }

                val density = LocalDensity.current

                val translationsDialogState =
                    translationsDialogStore.observeAsComposableState { it }.value

                val learnMoreUrl = SupportUtils.getSumoURLForTopic(
                    requireContext(),
                    SupportUtils.SumoTopic.TRANSLATIONS,
                )

                TranslationDialogBottomSheet {
                    TranslationsAnimation(
                        translationsVisibility = translationsVisibility,
                        density = density,
                        translationsOptionsHeightDp = translationsOptionsHeightDp,
                    ) {
                        if (translationsVisibility) {
                            Column(
                                modifier = Modifier.onGloballyPositioned { coordinates ->
                                    translationsHeightDp = with(density) {
                                        coordinates.size.height.toDp()
                                    }
                                    translationsWidthDp = with(density) {
                                        coordinates.size.width.toDp()
                                    }
                                },
                            ) {
                                TranslationsDialogContent(
                                    learnMoreUrl = learnMoreUrl,
                                    translationsDialogState = translationsDialogState,
                                ) {
                                    translationsVisibility = false
                                }
                            }
                        }
                    }

                    TranslationsOptionsAnimation(
                        translationsVisibility = !translationsVisibility,
                        density = density,
                        translationsHeightDp = translationsHeightDp,
                        translationsWidthDp = translationsWidthDp,
                    ) {
                        if (!translationsVisibility) {
                            Column(
                                modifier = Modifier.onGloballyPositioned { coordinates ->
                                    translationsOptionsHeightDp = with(density) {
                                        coordinates.size.height.toDp()
                                    }
                                },
                            ) {
                                TranslationsOptionsDialogContent(
                                    learnMoreUrl = learnMoreUrl,
                                    initialFrom = translationsDialogState?.initialFrom,
                                ) {
                                    translationsVisibility = true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        translationDialogBinding.set(
            feature = TranslationsDialogBinding(
                browserStore = browserStore,
                translationsDialogStore = translationsDialogStore,
                sessionId = args.sessionId,
                getTranslatedPageTitle = { localizedFrom, localizedTo ->
                    requireContext().getString(
                        R.string.translations_bottom_sheet_title_translation_completed,
                        localizedFrom,
                        localizedTo,
                    )
                },
            ),
            owner = this,
            view = view,
        )
        translationsDialogStore.dispatch(TranslationsDialogAction.InitTranslationsDialog)

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

    @Suppress("LongMethod")
    @Composable
    private fun TranslationsDialogContent(
        learnMoreUrl: String,
        translationsDialogState: TranslationsDialogState? = null,
        onSettingClicked: () -> Unit,
    ) {
        translationsDialogState?.let { state ->
            isTranslationInProgress = state.isTranslationInProgress

            if (state.dismissDialogState is DismissDialogState.Dismiss) {
                dismissDialog()
            }

            var showDownloadLanguageFileDialog by remember {
                mutableStateOf(false)
            }

            TranslationsDialog(
                translationsDialogState = translationsDialogState,
                learnMoreUrl = learnMoreUrl,
                showFirstTime = requireContext().settings().showFirstTimeTranslation,
                onSettingClicked = onSettingClicked,
                onLearnMoreClicked = { openBrowserAndLoad(learnMoreUrl) },
                onPositiveButtonClicked = {
                    if (state.error is TranslationError.CouldNotLoadLanguagesError) {
                        translationsDialogStore.dispatch(TranslationsDialogAction.FetchSupportedLanguages)
                    } else {
                        if (
                            isDataSaverEnabledAndWifiDisabled &&
                            !requireContext().settings().ignoreTranslationsDataSaverWarning &&
                            state.translationDownloadSize != null
                        ) {
                            showDownloadLanguageFileDialog = true
                        } else {
                            translationsDialogStore.dispatch(TranslationsDialogAction.TranslateAction)
                        }
                    }
                },
                onNegativeButtonClicked = {
                    if (state.isTranslated) {
                        translationsDialogStore.dispatch(TranslationsDialogAction.RestoreTranslation)
                    }
                    dismiss()
                },
                onFromSelected = { fromLanguage ->
                    state.initialTo?.let {
                        translationsDialogStore.dispatch(
                            TranslationsDialogAction.FetchDownloadFileSizeAction(
                                toLanguage = it,
                                fromLanguage = fromLanguage,
                            ),
                        )
                    }

                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateFromSelectedLanguage(
                            fromLanguage,
                        ),
                    )
                },
                onToSelected = { toLanguage ->
                    state.initialFrom?.let {
                        translationsDialogStore.dispatch(
                            TranslationsDialogAction.FetchDownloadFileSizeAction(
                                toLanguage = toLanguage,
                                fromLanguage = it,
                            ),
                        )
                    }

                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateToSelectedLanguage(
                            toLanguage,
                        ),
                    )
                },
            )

            var checkBoxEnabled by remember { mutableStateOf(false) }
            if (showDownloadLanguageFileDialog) {
                state.translationDownloadSize?.size?.let { fileSize ->
                    DownloadLanguageFileDialog(
                        downloadLanguageDialogType = DownloadLanguageFileDialogType.TranslationRequest,
                        fileSize = fileSize,
                        isCheckBoxEnabled = checkBoxEnabled,
                        onSavingModeStateChange = { checkBoxEnabled = it },
                        onConfirmDownload = {
                            requireContext().settings().ignoreTranslationsDataSaverWarning =
                                checkBoxEnabled
                            showDownloadLanguageFileDialog = false
                            translationsDialogStore.dispatch(TranslationsDialogAction.TranslateAction)
                        },
                        onCancel = { showDownloadLanguageFileDialog = false },
                    )
                }
            }
        }
    }

    @Composable
    private fun TranslationsOptionsDialogContent(
        learnMoreUrl: String,
        initialFrom: Language? = null,
        onBackClicked: () -> Unit,
    ) {
        val pageSettingsState =
            browserStore.observeAsComposableState { state ->
                state.findTab(args.sessionId)?.translationsState?.pageSettings
            }.value

        TranslationsOptionsDialog(
            context = requireContext(),
            translationPageSettings = pageSettingsState,
            initialFrom = initialFrom,
            onStateChange = { type, checked ->
                translationsDialogStore.dispatch(
                    TranslationsDialogAction.UpdatePageSettingsValue(
                        type as TranslationPageSettingsOption,
                        checked,
                    ),
                )
            },
            onBackClicked = onBackClicked,
            onTranslationSettingsClicked = {
                findNavController().navigate(
                    TranslationsDialogFragmentDirections
                        .actionTranslationsDialogFragmentToTranslationSettingsFragment(
                            sessionId = args.sessionId,
                        ),
                )
            },
            aboutTranslationClicked = {
                openBrowserAndLoad(learnMoreUrl)
            },
        )
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        if (isTranslationInProgress == true) {
            setFragmentResult(
                TRANSLATION_IN_PROGRESS,
                bundleOf(
                    SESSION_ID to args.sessionId,
                ),
            )
        }
    }

    private fun openBrowserAndLoad(learnMoreUrl: String) {
        (requireActivity() as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = learnMoreUrl,
            newTab = true,
            from = BrowserDirection.FromTranslationsDialogFragment,
        )
    }

    private fun dismissDialog() {
        if (requireContext().settings().showFirstTimeTranslation) {
            requireContext().settings().showFirstTimeTranslation = false
        }
        dismiss()
    }

    companion object {
        const val TRANSLATION_IN_PROGRESS = "translationInProgress"
        const val SESSION_ID = "sessionId"
    }
}
