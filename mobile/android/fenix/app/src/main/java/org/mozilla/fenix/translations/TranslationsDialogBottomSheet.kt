/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import android.content.res.Configuration
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.requiredSizeIn
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.material.Divider
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationError
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.BetaLabel
import org.mozilla.fenix.compose.ContextualMenu
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.TertiaryButton
import org.mozilla.fenix.compose.button.TextButton
import org.mozilla.fenix.shopping.ui.ReviewQualityCheckInfoCard
import org.mozilla.fenix.shopping.ui.ReviewQualityCheckInfoType
import org.mozilla.fenix.theme.FirefoxTheme
import java.util.Locale

private val ICON_SIZE = 24.dp

/**
 * Firefox Translations bottom sheet dialog.
 *
 * @param translationsDialogState The current state of the Translations bottom sheet dialog.
 * @param learnMoreUrl The learn more link for translations website.
 * @param showFirstTimeFlow Whether translations first flow should be shown.
 * @param onSettingClicked Invoked when the user clicks on the settings button.
 * @param onLearnMoreClicked Invoked when the user clicks on the "Learn More" button.
 * @param onPositiveButtonClicked Invoked when the user clicks on the positive button.
 * @param onNegativeButtonClicked Invoked when the user clicks on the negative button.
 * @param onFromDropdownSelected Invoked when the user selects an item on the from dropdown.
 * @param onToDropdownSelected Invoked when the user selects an item on the to dropdown.
 */
@Suppress("LongParameterList")
@Composable
fun TranslationsDialogBottomSheet(
    translationsDialogState: TranslationsDialogState,
    learnMoreUrl: String,
    showFirstTimeFlow: Boolean = false,
    onSettingClicked: () -> Unit,
    onLearnMoreClicked: () -> Unit,
    onPositiveButtonClicked: () -> Unit,
    onNegativeButtonClicked: () -> Unit,
    onFromDropdownSelected: (Language) -> Unit,
    onToDropdownSelected: (Language) -> Unit,
) {
    Column(
        modifier = Modifier.padding(16.dp),
    ) {
        BetaLabel(
            modifier = Modifier
                .padding(bottom = 8.dp)
                .clearAndSetSemantics {},
        )

        TranslationsDialogHeader(
            title = if (
                translationsDialogState.isTranslated && translationsDialogState.translatedPageTitle != null
            ) {
                translationsDialogState.translatedPageTitle
            } else {
                getTranslationsDialogTitle(
                    showFirstTime = showFirstTimeFlow,
                )
            },
            onSettingClicked = onSettingClicked,
        )

        Spacer(modifier = Modifier.height(8.dp))

        if (showFirstTimeFlow) {
            TranslationsDialogInfoMessage(
                learnMoreUrl = learnMoreUrl,
                onLearnMoreClicked = onLearnMoreClicked,
            )
        }

        DialogContentBaseOnTranslationState(
            translationsDialogState = translationsDialogState,
            learnMoreUrl = learnMoreUrl,
            onLearnMoreClicked = onLearnMoreClicked,
            onPositiveButtonClicked = onPositiveButtonClicked,
            onNegativeButtonClicked = onNegativeButtonClicked,
            onFromDropdownSelected = onFromDropdownSelected,
            onToDropdownSelected = onToDropdownSelected,
        )
    }
}

/**
 * Dialog content will adapt based on the [TranslationsDialogState].
 *
 * @param translationsDialogState The current state of the Translations bottom sheet dialog.
 * @param learnMoreUrl The learn more link for translations website.
 * @param onLearnMoreClicked Invoked when the user clicks on the learn more button.
 * @param onPositiveButtonClicked Invoked when the user clicks on the positive button.
 * @param onNegativeButtonClicked Invoked when the user clicks on the negative button.
 * @param onFromDropdownSelected Invoked when the user selects an item on the from dropdown.
 * @param onToDropdownSelected Invoked when the user selects an item on the to dropdown.
 */
@Composable
private fun DialogContentBaseOnTranslationState(
    translationsDialogState: TranslationsDialogState,
    learnMoreUrl: String,
    onLearnMoreClicked: () -> Unit,
    onPositiveButtonClicked: () -> Unit,
    onNegativeButtonClicked: () -> Unit,
    onFromDropdownSelected: (Language) -> Unit,
    onToDropdownSelected: (Language) -> Unit,
) {
    if (translationsDialogState.error != null) {
        DialogContentAnErrorOccurred(
            error = translationsDialogState.error,
            learnMoreUrl = learnMoreUrl,
            onLearnMoreClicked = onLearnMoreClicked,
            fromLanguages = translationsDialogState.fromLanguages,
            toLanguages = translationsDialogState.toLanguages,
            initialFrom = translationsDialogState.initialFrom,
            initialTo = translationsDialogState.initialTo,
            onFromDropdownSelected = onFromDropdownSelected,
            onToDropdownSelected = onToDropdownSelected,
            onPositiveButtonClicked = onPositiveButtonClicked,
            onNegativeButtonClicked = onNegativeButtonClicked,
        )
    } else if (translationsDialogState.isTranslated) {
        DialogContentTranslated(
            translateToLanguages = translationsDialogState.toLanguages,
            onFromDropdownSelected = onFromDropdownSelected,
            onToDropdownSelected = onToDropdownSelected,
            onPositiveButtonClicked = onPositiveButtonClicked,
            onNegativeButtonClicked = onNegativeButtonClicked,
            positiveButtonType = translationsDialogState.positiveButtonType,
            initialTo = translationsDialogState.initialTo,
        )
    } else {
        Spacer(modifier = Modifier.height(14.dp))

        TranslationsDialogContent(
            translateFromLanguages = translationsDialogState.fromLanguages,
            translateToLanguages = translationsDialogState.toLanguages,
            initialFrom = translationsDialogState.initialFrom,
            initialTo = translationsDialogState.initialTo,
            onFromDropdownSelected = onFromDropdownSelected,
            onToDropdownSelected = onToDropdownSelected,
        )

        Spacer(modifier = Modifier.height(16.dp))

        TranslationsDialogActionButtons(
            positiveButtonText = stringResource(id = R.string.translations_bottom_sheet_positive_button),
            negativeButtonText = stringResource(id = R.string.translations_bottom_sheet_negative_button),
            positiveButtonType = translationsDialogState.positiveButtonType,
            onNegativeButtonClicked = onNegativeButtonClicked,
            onPositiveButtonClicked = onPositiveButtonClicked,
        )
    }
}

/**
 * Dialog content if the web page was translated.
 *
 * @param translateToLanguages Translation menu items to be shown in the translate to dropdown.
 * @param onFromDropdownSelected Invoked when the user selects an item on the from dropdown.
 * @param onToDropdownSelected Invoked when the user selects an item on the to dropdown.
 * @param onPositiveButtonClicked Invoked when the user clicks on the positive button.
 * @param onNegativeButtonClicked Invoked when the user clicks on the negative button.
 * @param positiveButtonType Can be enabled,disabled or in progress. If it is null, the button will be disabled.
 * @param initialTo Initial "to" language, based on the translation state and page state.
 */
@Composable
private fun DialogContentTranslated(
    translateToLanguages: List<Language>?,
    onFromDropdownSelected: (Language) -> Unit,
    onToDropdownSelected: (Language) -> Unit,
    onPositiveButtonClicked: () -> Unit,
    onNegativeButtonClicked: () -> Unit,
    positiveButtonType: PositiveButtonType? = null,
    initialTo: Language? = null,
) {
    Spacer(modifier = Modifier.height(14.dp))

    TranslationsDialogContent(
        translateToLanguages = translateToLanguages,
        initialTo = initialTo,
        onFromDropdownSelected = onFromDropdownSelected,
        onToDropdownSelected = onToDropdownSelected,
    )

    Spacer(modifier = Modifier.height(16.dp))

    TranslationsDialogActionButtons(
        positiveButtonText = stringResource(id = R.string.translations_bottom_sheet_positive_button),
        negativeButtonText = stringResource(id = R.string.translations_bottom_sheet_negative_button_restore),
        positiveButtonType = positiveButtonType,
        onPositiveButtonClicked = onPositiveButtonClicked,
        onNegativeButtonClicked = onNegativeButtonClicked,
    )
}

/**
 * Dialog content if an [TranslationError] appears during the translation process.
 *
 * @param error An error that can occur during the translation process.
 * @param learnMoreUrl The learn more link for translations website.
 * @param onLearnMoreClicked Invoked when the user clicks on the learn more button.
 * @param fromLanguages Translation menu items to be shown in the translate from dropdown.
 * @param toLanguages Translation menu items to be shown in the translate to dropdown.
 * @param initialFrom Initial "from" language, based on the translation state and page state.
 * @param initialTo Initial "to" language, based on the translation state and page state.
 * @param onFromDropdownSelected Invoked when the user selects an item on the from dropdown.
 * @param onToDropdownSelected Invoked when the user selects an item on the to dropdown.
 * @param onPositiveButtonClicked Invoked when the user clicks on the positive button.
 * @param onNegativeButtonClicked Invoked when the user clicks on the negative button.
 */
@Suppress("LongParameterList")
@Composable
private fun DialogContentAnErrorOccurred(
    error: TranslationError,
    learnMoreUrl: String,
    onLearnMoreClicked: () -> Unit,
    fromLanguages: List<Language>?,
    toLanguages: List<Language>?,
    initialFrom: Language? = null,
    initialTo: Language? = null,
    onFromDropdownSelected: (Language) -> Unit,
    onToDropdownSelected: (Language) -> Unit,
    onPositiveButtonClicked: () -> Unit,
    onNegativeButtonClicked: () -> Unit,
) {
    TranslationErrorWarning(
        error,
        learnMoreUrl = learnMoreUrl,
        onLearnMoreClicked = onLearnMoreClicked,
    )

    Spacer(modifier = Modifier.height(14.dp))

    if (error !is TranslationError.CouldNotLoadLanguagesError) {
        TranslationsDialogContent(
            translateFromLanguages = fromLanguages,
            translateToLanguages = toLanguages,
            initialFrom = initialFrom,
            initialTo = initialTo,
            onFromDropdownSelected = onFromDropdownSelected,
            onToDropdownSelected = onToDropdownSelected,
        )
    }

    val negativeButtonTitle = if (error is TranslationError.LanguageNotSupportedError) {
        stringResource(id = R.string.translations_bottom_sheet_negative_button_error)
    } else {
        stringResource(id = R.string.translations_bottom_sheet_negative_button)
    }

    val positiveButtonTitle = if (error is TranslationError.CouldNotLoadLanguagesError) {
        stringResource(id = R.string.translations_bottom_sheet_positive_button_error)
    } else {
        stringResource(id = R.string.translations_bottom_sheet_positive_button)
    }

    val positiveButtonType = if (error is TranslationError.LanguageNotSupportedError) {
        PositiveButtonType.Disabled
    } else {
        PositiveButtonType.Enabled
    }

    TranslationsDialogActionButtons(
        positiveButtonText = positiveButtonTitle,
        negativeButtonText = negativeButtonTitle,
        positiveButtonType = positiveButtonType,
        onNegativeButtonClicked = onNegativeButtonClicked,
        onPositiveButtonClicked = onPositiveButtonClicked,
    )
}

@Composable
private fun TranslationsDialogContent(
    translateFromLanguages: List<Language>? = null,
    translateToLanguages: List<Language>? = null,
    initialFrom: Language? = null,
    initialTo: Language? = null,
    onFromDropdownSelected: (Language) -> Unit,
    onToDropdownSelected: (Language) -> Unit,
) {
    var orientation by remember { mutableIntStateOf(Configuration.ORIENTATION_PORTRAIT) }

    val configuration = LocalConfiguration.current

    LaunchedEffect(configuration) {
        snapshotFlow { configuration.orientation }.collect { orientation = it }
    }
    when (orientation) {
        Configuration.ORIENTATION_LANDSCAPE -> {
            TranslationsDialogContentInLandscapeMode(
                translateFromLanguages = translateFromLanguages,
                translateToLanguages = translateToLanguages,
                initialFrom = initialFrom,
                initialTo = initialTo,
                onFromDropdownSelected = onFromDropdownSelected,
                onToDropdownSelected = onToDropdownSelected,
            )
        }

        else -> {
            TranslationsDialogContentInPortraitMode(
                translateFromLanguages = translateFromLanguages,
                translateToLanguages = translateToLanguages,
                initialFrom = initialFrom,
                initialTo = initialTo,
                onFromDropdownSelected = onFromDropdownSelected,
                onToDropdownSelected = onToDropdownSelected,
            )
        }
    }

    Spacer(modifier = Modifier.height(16.dp))
}

@Composable
private fun TranslationsDialogContentInPortraitMode(
    translateFromLanguages: List<Language>? = null,
    translateToLanguages: List<Language>? = null,
    initialFrom: Language? = null,
    initialTo: Language? = null,
    onFromDropdownSelected: (Language) -> Unit,
    onToDropdownSelected: (Language) -> Unit,
) {
    Column {
        translateFromLanguages?.let {
            TranslationsDropdown(
                header = stringResource(id = R.string.translations_bottom_sheet_translate_from),
                modifier = Modifier.fillMaxWidth(),
                isInLandscapeMode = false,
                translateLanguages = translateFromLanguages,
                initiallySelected = initialFrom,
                onLanguageSelection = onFromDropdownSelected,
            )

            Spacer(modifier = Modifier.height(16.dp))
        }

        translateToLanguages?.let {
            TranslationsDropdown(
                header = stringResource(id = R.string.translations_bottom_sheet_translate_to),
                modifier = Modifier.fillMaxWidth(),
                isInLandscapeMode = false,
                translateLanguages = it,
                initiallySelected = initialTo,
                onLanguageSelection = onToDropdownSelected,
            )
        }
    }
}

@Composable
private fun TranslationsDialogContentInLandscapeMode(
    translateFromLanguages: List<Language>? = null,
    translateToLanguages: List<Language>? = null,
    initialFrom: Language? = null,
    initialTo: Language? = null,
    onFromDropdownSelected: (Language) -> Unit,
    onToDropdownSelected: (Language) -> Unit,
) {
    Column {
        Row {
            translateFromLanguages?.let {
                TranslationsDropdown(
                    header = stringResource(id = R.string.translations_bottom_sheet_translate_from),
                    modifier = Modifier.weight(1f),
                    isInLandscapeMode = true,
                    translateLanguages = translateFromLanguages,
                    initiallySelected = initialFrom,
                    onLanguageSelection = onFromDropdownSelected,
                )

                Spacer(modifier = Modifier.width(16.dp))
            }

            translateToLanguages?.let {
                TranslationsDropdown(
                    header = stringResource(id = R.string.translations_bottom_sheet_translate_to),
                    modifier = Modifier.weight(1f),
                    isInLandscapeMode = true,
                    translateLanguages = it,
                    initiallySelected = initialTo,
                    onLanguageSelection = onToDropdownSelected,
                )
            }
        }
    }
}

@Composable
private fun TranslationsDialogHeader(
    title: String,
    onSettingClicked: () -> Unit,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = title,
            modifier = Modifier
                .weight(1f)
                .semantics { heading() },
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline7,
        )

        Spacer(modifier = Modifier.width(4.dp))

        IconButton(
            onClick = { onSettingClicked() },
            modifier = Modifier.size(24.dp),
        ) {
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_settings_24),
                contentDescription = stringResource(id = R.string.translation_option_bottom_sheet_title),
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }
    }
}

@Composable
private fun TranslationErrorWarning(
    translationError: TranslationError,
    learnMoreUrl: String,
    onLearnMoreClicked: () -> Unit,
) {
    val modifier = Modifier
        .padding(top = 8.dp)
        .fillMaxWidth()

    when (translationError) {
        is TranslationError.CouldNotTranslateError -> {
            ReviewQualityCheckInfoCard(
                title = stringResource(id = R.string.translation_error_could_not_translate_warning_text),
                type = ReviewQualityCheckInfoType.Error,
                modifier = modifier,
            )
        }

        is TranslationError.CouldNotLoadLanguagesError -> {
            ReviewQualityCheckInfoCard(
                title = stringResource(id = R.string.translation_error_could_not_load_languages_warning_text),
                type = ReviewQualityCheckInfoType.Error,
                modifier = modifier,
            )
        }

        is TranslationError.LanguageNotSupportedError -> {
            val learnMoreText =
                stringResource(id = R.string.translation_error_language_not_supported_learn_more)

            ReviewQualityCheckInfoCard(
                title = stringResource(
                    id = R.string.translation_error_language_not_supported_warning_text,
                    "Uzbek",
                ),
                type = ReviewQualityCheckInfoType.Info,
                modifier = modifier,
                footer = stringResource(
                    id = R.string.translation_error_language_not_supported_learn_more,
                ) to LinkTextState(
                    text = learnMoreText,
                    url = learnMoreUrl,
                    onClick = { onLearnMoreClicked() },
                ),
            )
        }

        else -> {}
    }
}

@Composable
private fun TranslationsDialogInfoMessage(
    learnMoreUrl: String,
    onLearnMoreClicked: () -> Unit,
) {
    val learnMoreText =
        stringResource(id = R.string.translations_bottom_sheet_info_message_learn_more)

    val learnMoreState = LinkTextState(
        text = learnMoreText,
        url = learnMoreUrl,
        onClick = { onLearnMoreClicked() },
    )

    Box {
        LinkText(
            text = stringResource(
                R.string.translations_bottom_sheet_info_message,
                learnMoreText,
            ),
            linkTextStates = listOf(learnMoreState),
            style = FirefoxTheme.typography.subtitle1.copy(
                color = FirefoxTheme.colors.textPrimary,
            ),
            linkTextDecoration = TextDecoration.Underline,
        )
    }
}

@Suppress("LongMethod")
@Composable
private fun TranslationsDropdown(
    header: String,
    translateLanguages: List<Language>,
    modifier: Modifier = Modifier,
    isInLandscapeMode: Boolean,
    initiallySelected: Language? = null,
    onLanguageSelection: (Language) -> Unit,
) {
    val density = LocalDensity.current

    var expanded by remember { mutableStateOf(false) }

    var contextMenuWidthDp by remember {
        mutableStateOf(0.dp)
    }

    Column(
        modifier = modifier
            .clickable {
                expanded = true
            }
            .semantics { role = Role.DropdownList },
    ) {
        Text(
            text = header,
            modifier = Modifier.wrapContentSize(),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.caption,
        )

        Spacer(modifier = Modifier.height(4.dp))

        var initialValue =
            stringResource(R.string.translations_bottom_sheet_default_dropdown_selection)
        initiallySelected?.localizedDisplayName?.let {
            initialValue = it
        }

        Row {
            Text(
                text = initialValue,
                modifier = Modifier.weight(1f),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.subtitle1,
            )

            Spacer(modifier = Modifier.width(10.dp))

            Box {
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_dropdown_arrow),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                )

                ContextualMenu(
                    showMenu = expanded,
                    onDismissRequest = {
                        expanded = false
                    },
                    menuItems = getContextMenuItems(
                        translateLanguages = translateLanguages,
                        selectedLanguage = initiallySelected,
                        onClickItem = {
                            onLanguageSelection(it)
                        },
                    ),
                    modifier = Modifier
                        .onGloballyPositioned { coordinates ->
                            contextMenuWidthDp = with(density) {
                                coordinates.size.width.toDp()
                            }
                        }
                        .requiredSizeIn(maxHeight = 200.dp)
                        .padding(horizontal = if (initiallySelected == null) 36.dp else 4.dp),
                    offset = if (isInLandscapeMode) {
                        DpOffset(
                            -contextMenuWidthDp + ICON_SIZE,
                            -ICON_SIZE,
                        )
                    } else {
                        DpOffset(
                            0.dp,
                            -ICON_SIZE,
                        )
                    },
                )
            }
        }

        Divider(color = FirefoxTheme.colors.formDefault)
    }
}

@Composable
private fun getTranslationsDialogTitle(
    showFirstTime: Boolean = false,
) = if (showFirstTime) {
    stringResource(
        id = R.string.translations_bottom_sheet_title_first_time,
        stringResource(id = R.string.firefox),
    )
} else {
    stringResource(id = R.string.translations_bottom_sheet_title)
}

private fun getContextMenuItems(
    translateLanguages: List<Language>,
    selectedLanguage: Language? = null,
    onClickItem: (Language) -> Unit,
): List<MenuItem> {
    val menuItems = mutableListOf<MenuItem>()
    translateLanguages.map { item ->
        item.localizedDisplayName?.let {
            menuItems.add(
                MenuItem(
                    title = it,
                    isChecked = item == selectedLanguage,
                    onClick = {
                        onClickItem(item)
                    },
                ),
            )
        }
    }
    return menuItems
}

@Composable
private fun TranslationsDialogActionButtons(
    positiveButtonText: String,
    negativeButtonText: String,
    positiveButtonType: PositiveButtonType? = null,
    onPositiveButtonClicked: () -> Unit,
    onNegativeButtonClicked: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.End,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        TextButton(
            text = negativeButtonText,
            modifier = Modifier,
            onClick = onNegativeButtonClicked,
        )

        Spacer(modifier = Modifier.width(10.dp))

        when (positiveButtonType) {
            PositiveButtonType.InProgress -> {
                DownloadIndicator(
                    text = positiveButtonText,
                    contentDescription = stringResource(
                        id = R.string.translations_bottom_sheet_translating_in_progress_content_description,
                    ),
                    icon = painterResource(id = R.drawable.mozac_ic_sync_24),
                )
            }

            PositiveButtonType.Enabled -> {
                PrimaryButton(
                    text = positiveButtonText,
                    modifier = Modifier.wrapContentSize(),
                ) {
                    onPositiveButtonClicked()
                }
            }

            else -> {
                TertiaryButton(
                    text = positiveButtonText,
                    enabled = false,
                    modifier = Modifier.wrapContentSize(),
                ) {
                    onPositiveButtonClicked()
                }
            }
        }
    }
}

@Composable
@LightDarkPreview
private fun TranslationsDialogBottomSheetPreview() {
    FirefoxTheme {
        TranslationsDialogBottomSheet(
            translationsDialogState = TranslationsDialogState(
                positiveButtonType = PositiveButtonType.Enabled,
                toLanguages = getTranslateToLanguageList(),
                fromLanguages = getTranslateFromLanguageList(),
            ),
            learnMoreUrl = "",
            showFirstTimeFlow = true,
            onSettingClicked = {},
            onLearnMoreClicked = {},
            onPositiveButtonClicked = {},
            onNegativeButtonClicked = {},
            onFromDropdownSelected = {},
            onToDropdownSelected = {},
        )
    }
}

@Composable
internal fun getTranslateFromLanguageList(): List<Language> {
    return mutableListOf<Language>().apply {
        add(
            Language(
                code = Locale.ENGLISH.toLanguageTag(),
                localizedDisplayName = Locale.ENGLISH.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.FRENCH.toLanguageTag(),
                localizedDisplayName = Locale.FRENCH.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.GERMAN.toLanguageTag(),
                localizedDisplayName = Locale.GERMAN.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.ITALIAN.toLanguageTag(),
                localizedDisplayName = Locale.ITALIAN.displayLanguage,
            ),
        )
    }
}

@Composable
internal fun getTranslateToLanguageList(): List<Language> {
    return mutableListOf<Language>().apply {
        add(
            Language(
                code = Locale.ENGLISH.toLanguageTag(),
                localizedDisplayName = Locale.ENGLISH.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.FRENCH.toLanguageTag(),
                localizedDisplayName = Locale.FRENCH.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.GERMAN.toLanguageTag(),
                localizedDisplayName = Locale.GERMAN.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.ITALIAN.toLanguageTag(),
                localizedDisplayName = Locale.ITALIAN.displayLanguage,
            ),
        )
    }
}
