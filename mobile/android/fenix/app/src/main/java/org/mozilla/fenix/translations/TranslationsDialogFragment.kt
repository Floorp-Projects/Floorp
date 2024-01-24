/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.initialFromLanguage
import mozilla.components.concept.engine.translate.initialToLanguage
import mozilla.components.lib.state.ext.observeAsComposableState
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
        // Signalling user intention to translate
        browserStore.dispatch(TranslationsAction.TranslateExpectedAction(args.sessionId))

        setContent {
            val translationsState = browserStore.observeAsComposableState {
                    state ->
                state.findTab(args.sessionId)
                    ?.translationsState
            }.value

            var fromSelected by remember {
                mutableStateOf(
                    translationsState?.translationEngineState
                        ?.initialFromLanguage(translationsState.supportedLanguages?.fromLanguages),
                )
            }

            var toSelected by remember {
                mutableStateOf(
                    translationsState?.translationEngineState
                        ?.initialToLanguage(translationsState.supportedLanguages?.toLanguages),
                )
            }

            FirefoxTheme {
                var translationsVisibility by remember {
                    mutableStateOf(
                        args.translationsDialogAccessPoint ==
                            TranslationsDialogAccessPoint.Translations,
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
                                    context,
                                    SupportUtils.SumoTopic.TRANSLATIONS,
                                )
                                TranslationsDialog(
                                    learnMoreUrl = learnMoreUrl,
                                    showFirstTimeTranslation = context.settings().showFirstTimeTranslation,
                                    translateFromLanguages = translationsState?.supportedLanguages?.fromLanguages,
                                    translateToLanguages = translationsState?.supportedLanguages?.toLanguages,
                                    initialFrom = fromSelected,
                                    initialTo = toSelected,
                                    onSettingClicked = {
                                        translationsVisibility = false
                                    },
                                    onLearnMoreClicked = {
                                        (requireActivity() as HomeActivity).openToBrowserAndLoad(
                                            searchTermOrURL = learnMoreUrl,
                                            newTab = true,
                                            from = BrowserDirection.FromTranslationsDialogFragment,
                                        )
                                    },
                                    onTranslateButtonClick = {
                                        fromSelected?.code?.let { fromLanguage ->
                                            toSelected?.code?.let { toLanguage ->
                                                TranslationsAction.TranslateAction(
                                                    tabId = args.sessionId,
                                                    fromLanguage = fromLanguage,
                                                    toLanguage = toLanguage,
                                                    options = null,
                                                )
                                            }
                                        }?.let {
                                            browserStore.dispatch(
                                                it,
                                            )
                                        }
                                    },
                                    onNotNowButtonClick = { dismiss() },
                                    onFromSelected = { fromSelected = it },
                                    onToSelected = { toSelected = it },
                                )
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
}
