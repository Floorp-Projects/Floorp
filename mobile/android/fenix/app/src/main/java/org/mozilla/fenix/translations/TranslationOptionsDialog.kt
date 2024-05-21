/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import mozilla.components.concept.engine.translate.TranslationError
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.shopping.ui.ReviewQualityCheckInfoCard
import org.mozilla.fenix.shopping.ui.ReviewQualityCheckInfoType
import org.mozilla.fenix.theme.FirefoxTheme
import java.util.Locale

/**
 * Firefox Translation options bottom sheet dialog.
 *
 * @param translationOptionsList A list of [TranslationSwitchItem]s to display.
 * @param showGlobalSettings Whether to show to global settings entry point or not.
 * @param pageSettingsError Could not load page settings error.
 * @param onBackClicked Invoked when the user clicks on the back button.
 * @param onTranslationSettingsClicked Invoked when the user clicks on the "Translation Settings" button.
 * @param aboutTranslationClicked Invoked when the user clicks on the "About Translation" button.
 */
@Composable
fun TranslationOptionsDialog(
    translationOptionsList: List<TranslationSwitchItem>,
    showGlobalSettings: Boolean,
    pageSettingsError: TranslationError? = null,
    onBackClicked: () -> Unit,
    onTranslationSettingsClicked: () -> Unit,
    aboutTranslationClicked: () -> Unit,
) {
    TranslationOptionsDialogHeader(onBackClicked)

    pageSettingsError?.let {
        TranslationPageSettingsErrorWarning()
    }

    translationOptionsList.forEach { item: TranslationSwitchItem ->
        Column {
            val translationSwitchItem = TranslationSwitchItem(
                type = item.type,
                textLabel = item.textLabel,
                isChecked = item.isChecked,
                isEnabled = item.isEnabled,
                onStateChange = { translationPageSettingsOption, checked ->
                    item.onStateChange.invoke(translationPageSettingsOption, checked)
                },
            )
            TranslationOptions(
                translationSwitchItem = translationSwitchItem,
            )
        }
    }

    if (showGlobalSettings) {
        Column {
            TextListItem(
                label = stringResource(id = R.string.translation_option_bottom_sheet_translation_settings),
                modifier = Modifier
                    .padding(start = 56.dp)
                    .defaultMinSize(minHeight = 56.dp)
                    .wrapContentHeight(),
                onClick = {
                    onTranslationSettingsClicked()
                },
            )
        }
    }

    Column {
        TextListItem(
            label = stringResource(
                id = R.string.translation_option_bottom_sheet_about_translations,
                formatArgs = arrayOf(stringResource(R.string.firefox)),
            ),
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 56.dp)
                .defaultMinSize(minHeight = 56.dp)
                .wrapContentHeight(),
            onClick = { aboutTranslationClicked() },
        )

        Spacer(modifier = Modifier.height(16.dp))
    }
}

@Composable
private fun TranslationPageSettingsErrorWarning() {
    val modifier = Modifier
        .fillMaxWidth()
        .padding(start = 72.dp, end = 16.dp, bottom = 16.dp)
        .defaultMinSize(minHeight = 56.dp)
        .wrapContentHeight()

    ReviewQualityCheckInfoCard(
        description = stringResource(id = R.string.translation_option_bottom_sheet_error_warning_text),
        type = ReviewQualityCheckInfoType.Warning,
        verticalRowAlignment = Alignment.CenterVertically,
        modifier = modifier,
    )
}

@Composable
private fun TranslationOptions(
    translationSwitchItem: TranslationSwitchItem,
) {
    SwitchWithLabel(
        label = translationSwitchItem.textLabel,
        description = if (translationSwitchItem.isChecked) {
            translationSwitchItem.type.descriptionId?.let {
                stringResource(
                    id = it,
                )
            }
        } else {
            null
        },
        enabled = translationSwitchItem.isEnabled,
        checked = translationSwitchItem.isChecked,
        onCheckedChange = { checked ->
            translationSwitchItem.onStateChange.invoke(
                translationSwitchItem.type,
                checked,
            )
        },
        modifier = Modifier.padding(start = 72.dp, end = 16.dp, top = 6.dp, bottom = 6.dp),
    )

    if (translationSwitchItem.type.hasDivider) {
        Divider(Modifier.padding(top = 4.dp, bottom = 4.dp))
    }
}

@Composable
private fun TranslationOptionsDialogHeader(
    onBackClicked: () -> Unit,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .padding(end = 16.dp, start = 16.dp)
            .defaultMinSize(minHeight = 56.dp),
    ) {
        IconButton(
            onClick = { onBackClicked() },
            modifier = Modifier.size(24.dp),
        ) {
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_back_24),
                contentDescription = stringResource(R.string.etp_back_button_content_description),
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }

        Spacer(modifier = Modifier.width(32.dp))

        Text(
            text = stringResource(id = R.string.translation_option_bottom_sheet_title_heading),
            modifier = Modifier
                .weight(1f)
                .semantics { heading() },
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline7,
        )
    }
}

/**
 * Return a list of Translation option switch list item.
 */
@Composable
fun getTranslationOptionsList(): List<TranslationSwitchItem> {
    return mutableListOf<TranslationSwitchItem>().apply {
        add(
            TranslationSwitchItem(
                type = TranslationPageSettingsOption.AlwaysOfferPopup(),
                textLabel = stringResource(R.string.translation_option_bottom_sheet_always_translate),
                isChecked = false,
                isEnabled = true,
                onStateChange = { _, _ -> },
            ),
        )
        add(
            TranslationSwitchItem(
                type = TranslationPageSettingsOption.AlwaysTranslateLanguage(),
                textLabel = stringResource(
                    id = R.string.translation_option_bottom_sheet_always_translate_in_language,
                    formatArgs = arrayOf(Locale("es").displayName),
                ),
                isChecked = false,
                isEnabled = true,
                onStateChange = { _, _ -> },
            ),
        )
        add(
            TranslationSwitchItem(
                type = TranslationPageSettingsOption.NeverTranslateLanguage(),
                textLabel = stringResource(
                    id = R.string.translation_option_bottom_sheet_never_translate_in_language,
                    formatArgs = arrayOf(Locale("es").displayName),
                ),
                isChecked = true,
                isEnabled = true,
                onStateChange = { _, _ -> },
            ),
        )
        add(
            TranslationSwitchItem(
                type = TranslationPageSettingsOption.NeverTranslateSite(),
                textLabel = stringResource(R.string.translation_option_bottom_sheet_never_translate_site),
                isChecked = true,
                isEnabled = true,
                onStateChange = { _, _ -> },
            ),
        )
    }
}

@Composable
@LightDarkPreview
private fun TranslationSettingsPreview() {
    FirefoxTheme {
        Column {
            TranslationOptionsDialog(
                translationOptionsList = getTranslationOptionsList(),
                showGlobalSettings = true,
                onBackClicked = {},
                onTranslationSettingsClicked = {},
                aboutTranslationClicked = {},
            )
        }
    }
}
