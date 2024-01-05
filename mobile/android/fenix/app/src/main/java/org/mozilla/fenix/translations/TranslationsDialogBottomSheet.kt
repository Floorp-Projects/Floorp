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
 * @param learnMoreUrl The learn more link for translations website.
 * @param showFirstTimeTranslation Whether translations first flow should be shown.
 * @param translationError The type of translation errors that can occur.
 * @param translateFromLanguages Translation menu items to be shown in the translate from dropdown.
 * @param translateToLanguages Translation menu items are to be shown in the translate to dropdown.
 * @param onSettingClicked Invoked when the user clicks on the settings button.
 * @param onLearnMoreClicked Invoked when the user clicks on the "Learn More" button.
 * @param onTranslateButtonClicked Invoked when the user clicks on the "Translate" button.
 * @param onNotNowButtonClicked Invoked when the user clicks on the "Not Now" button.
 */
@Composable
@Suppress("LongParameterList")
fun TranslationsDialogBottomSheet(
    learnMoreUrl: String,
    showFirstTimeTranslation: Boolean,
    translationError: TranslationError? = null,
    translateFromLanguages: List<Language>,
    translateToLanguages: List<Language>,
    onSettingClicked: () -> Unit,
    onLearnMoreClicked: () -> Unit,
    onTranslateButtonClicked: () -> Unit,
    onNotNowButtonClicked: () -> Unit,
) {
    var orientation by remember { mutableIntStateOf(Configuration.ORIENTATION_PORTRAIT) }

    val configuration = LocalConfiguration.current

    LaunchedEffect(configuration) {
        snapshotFlow { configuration.orientation }.collect { orientation = it }
    }

    Column(
        modifier = Modifier.padding(16.dp),
    ) {
        BetaLabel(
            modifier = Modifier
                .padding(bottom = 8.dp)
                .clearAndSetSemantics {},
        )

        TranslationsDialogHeader(
            showFirstTimeTranslation = showFirstTimeTranslation,
            onSettingClicked = onSettingClicked,
        )

        Spacer(modifier = Modifier.height(8.dp))

        if (showFirstTimeTranslation) {
            TranslationsDialogInfoMessage(
                learnMoreUrl = learnMoreUrl,
                onLearnMoreClicked = onLearnMoreClicked,
            )
        }

        translationError?.let {
            TranslationErrorWarning(translationError)
        }

        Spacer(modifier = Modifier.height(14.dp))

        if (translationError !is TranslationError.CouldNotLoadLanguagesError) {
            when (orientation) {
                Configuration.ORIENTATION_LANDSCAPE -> {
                    TranslationsDialogContentInLandscapeMode(
                        translateFromLanguages = translateFromLanguages,
                        translateToLanguages = translateToLanguages,
                    )
                }

                else -> {
                    TranslationsDialogContentInPortraitMode(
                        translateFromLanguages = translateFromLanguages,
                        translateToLanguages = translateToLanguages,
                    )
                }
            }

            Spacer(modifier = Modifier.height(16.dp))
        }

        TranslationsDialogActionButtons(
            translationError = translationError,
            onTranslateButtonClicked = onTranslateButtonClicked,
            onNotNowButtonClicked = onNotNowButtonClicked,
        )
    }
}

@Composable
private fun TranslationsDialogContentInPortraitMode(
    translateFromLanguages: List<Language>,
    translateToLanguages: List<Language>,
) {
    Column {
        TranslationsDropdown(
            header = stringResource(id = R.string.translations_bottom_sheet_translate_from),
            modifier = Modifier.fillMaxWidth(),
            translateLanguages = translateFromLanguages,
        )

        Spacer(modifier = Modifier.height(16.dp))

        TranslationsDropdown(
            header = stringResource(id = R.string.translations_bottom_sheet_translate_to),
            modifier = Modifier.fillMaxWidth(),
            translateLanguages = translateToLanguages,
        )
    }
}

@Composable
private fun TranslationsDialogContentInLandscapeMode(
    translateFromLanguages: List<Language>,
    translateToLanguages: List<Language>,
) {
    Column {
        Row {
            TranslationsDropdown(
                header = stringResource(id = R.string.translations_bottom_sheet_translate_from),
                modifier = Modifier.weight(1f),
                isInLandscapeMode = true,
                translateLanguages = translateFromLanguages,
            )

            Spacer(modifier = Modifier.width(16.dp))

            TranslationsDropdown(
                header = stringResource(id = R.string.translations_bottom_sheet_translate_to),
                modifier = Modifier.weight(1f),
                isInLandscapeMode = true,
                translateLanguages = translateToLanguages,
            )
        }
    }
}

@Composable
private fun TranslationsDialogHeader(
    showFirstTimeTranslation: Boolean,
    onSettingClicked: () -> Unit,
) {
    val title: String = if (showFirstTimeTranslation) {
        stringResource(
            id = R.string.translations_bottom_sheet_title_first_time,
            stringResource(id = R.string.firefox),
        )
    } else {
        stringResource(id = R.string.translations_bottom_sheet_title)
    }

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
private fun TranslationErrorWarning(translationError: TranslationError) {
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
                    text = stringResource(id = R.string.translation_error_language_not_supported_learn_more),
                    url = "https://www.mozilla.org",
                    onClick = {},
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

@Composable
private fun TranslationsDropdown(
    header: String,
    translateLanguages: List<Language>,
    modifier: Modifier = Modifier,
    isInLandscapeMode: Boolean = false,
) {
    val density = LocalDensity.current

    var expanded by remember { mutableStateOf(false) }

    var selectedLanguage by remember {
        mutableStateOf(
            translateLanguages.last().localizedDisplayName,
        )
    }

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

        Row {
            selectedLanguage?.let {
                Text(
                    text = it,
                    modifier = Modifier.weight(1f),
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.subtitle1,
                )
            }

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
                    menuItems = getContextMenuItems(translateLanguages = translateLanguages) {
                        expanded = false
                        selectedLanguage = it.localizedDisplayName
                    },
                    modifier = Modifier
                        .onGloballyPositioned { coordinates ->
                            contextMenuWidthDp = with(density) {
                                coordinates.size.width.toDp()
                            }
                        }
                        .requiredSizeIn(maxHeight = 200.dp),
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

private fun getContextMenuItems(
    translateLanguages: List<Language>,
    onClickItem: (Language) -> Unit,
): List<MenuItem> {
    val menuItems = mutableListOf<MenuItem>()
    translateLanguages.map { item ->
        item.localizedDisplayName?.let {
            menuItems.add(
                MenuItem(
                    title = it,
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
    translationError: TranslationError? = null,
    onTranslateButtonClicked: () -> Unit,
    onNotNowButtonClicked: () -> Unit,
) {
    val isTranslationInProgress = remember { mutableStateOf(false) }

    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.End,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val negativeButtonTitle =
            if (translationError is TranslationError.LanguageNotSupportedError) {
                stringResource(id = R.string.translations_bottom_sheet_negative_button_error)
            } else {
                stringResource(id = R.string.translations_bottom_sheet_negative_button)
            }

        TextButton(
            text = negativeButtonTitle,
            modifier = Modifier,
            onClick = onNotNowButtonClicked,
        )

        Spacer(modifier = Modifier.width(10.dp))

        if (isTranslationInProgress.value) {
            DownloadIndicator(
                text = stringResource(id = R.string.translations_bottom_sheet_translating_in_progress),
                contentDescription = stringResource(
                    id = R.string.translations_bottom_sheet_translating_in_progress_content_description,
                ),
                icon = painterResource(id = R.drawable.mozac_ic_sync_24),
            )
        } else {
            val positiveButtonTitle =
                if (translationError is TranslationError.CouldNotLoadLanguagesError) {
                    stringResource(id = R.string.translations_bottom_sheet_positive_button_error)
                } else {
                    stringResource(id = R.string.translations_bottom_sheet_positive_button)
                }

            if (translationError is TranslationError.LanguageNotSupportedError) {
                TertiaryButton(
                    text = positiveButtonTitle,
                    enabled = false,
                    modifier = Modifier.wrapContentSize(),
                ) {
                    isTranslationInProgress.value = true
                    onTranslateButtonClicked()
                }
            } else {
                PrimaryButton(
                    text = positiveButtonTitle,
                    modifier = Modifier.wrapContentSize(),
                ) {
                    isTranslationInProgress.value = true
                    onTranslateButtonClicked()
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
            learnMoreUrl = "",
            showFirstTimeTranslation = true,
            translationError = TranslationError.LanguageNotSupportedError(null),
            translateFromLanguages = getTranslateFromLanguageList(),
            translateToLanguages = getTranslateToLanguageList(),
            onSettingClicked = {},
            onLearnMoreClicked = {},
            onTranslateButtonClicked = {},
            onNotNowButtonClicked = {},
        )
    }
}

@Composable
internal fun getTranslateFromLanguageList(): List<Language> {
    return mutableListOf<Language>().apply {
        add(
            Language(
                code = Locale.CHINA.toLanguageTag(),
                localizedDisplayName = Locale.CHINA.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.ENGLISH.toLanguageTag(),
                localizedDisplayName = Locale.ENGLISH.displayLanguage,
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
                code = Locale.JAPANESE.toLanguageTag(),
                localizedDisplayName = Locale.JAPANESE.displayLanguage,
            ),
        )
    }
}

@Composable
internal fun getTranslateToLanguageList(): List<Language> {
    return mutableListOf<Language>().apply {
        add(
            Language(
                code = Locale.KOREAN.toLanguageTag(),
                localizedDisplayName = Locale.KOREAN.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.CANADA.toLanguageTag(),
                localizedDisplayName = Locale.CANADA.displayLanguage,
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
                code = Locale.ITALY.toLanguageTag(),
                localizedDisplayName = Locale.ITALY.displayLanguage,
            ),
        )
        add(
            Language(
                code = Locale.GERMAN.toLanguageTag(),
                localizedDisplayName = Locale.GERMAN.displayLanguage,
            ),
        )
    }
}
