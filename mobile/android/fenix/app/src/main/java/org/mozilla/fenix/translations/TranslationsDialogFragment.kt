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
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.FirefoxTheme

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
    private lateinit var translationsDialogStore: TranslationsDialogStore
    private var isTranslationInProgress: Boolean? = null

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
                                val learnMoreUrl = SupportUtils.getSumoURLForTopic(
                                    requireContext(),
                                    SupportUtils.SumoTopic.TRANSLATIONS,
                                )

                                TranslationsDialogContent(
                                    learnMoreUrl = learnMoreUrl,
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
                                TranslationsOptionsDialog(
                                    onBackClicked = {
                                        translationsVisibility = true
                                    },
                                    onTranslationSettingsClicked = {
                                        findNavController().navigate(
                                            TranslationsDialogFragmentDirections
                                                .actionTranslationsDialogFragmentToTranslationSettingsFragment(
                                                    sessionId = args.sessionId,
                                                ),
                                        )
                                    },
                                )
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
    }

    @Composable
    private fun TranslationsDialogContent(learnMoreUrl: String, onSettingClicked: () -> Unit) {
        val translationsDialogState =
            translationsDialogStore.observeAsComposableState { it }.value

        translationsDialogState?.let { state ->
            isTranslationInProgress = state.isTranslationInProgress

            if (state.dismissDialogState is DismissDialogState.Dismiss) {
                dismissDialog()
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
                        translationsDialogStore.dispatch(TranslationsDialogAction.TranslateAction)
                    }
                },
                onNegativeButtonClicked = {
                    if (state.isTranslated) {
                        translationsDialogStore.dispatch(TranslationsDialogAction.RestoreTranslation)
                    }
                    dismiss()
                },
                onFromSelected = {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateFromSelectedLanguage(
                            it,
                        ),
                    )
                },
                onToSelected = {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateToSelectedLanguage(
                            it,
                        ),
                    )
                },
            )
        }
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
