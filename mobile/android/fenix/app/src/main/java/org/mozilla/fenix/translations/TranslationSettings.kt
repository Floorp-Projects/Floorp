/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
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

/**
 * Translation Settings Fragment.
 *
 * @param translationSwitchList list of [TranslationSwitchItem]s to display.
 * @param showAutomaticTranslations Show the entry point for the user to change automatic language settings.
 * @param showNeverTranslate Show the entry point for the user to change never translate settings.
 * @param showDownloads Show the entry point for the user to manage language models.
 * @param pageSettingsError Could not load page settings error.
 * @param onAutomaticTranslationClicked Invoked when the user clicks on the "Automatic Translation" button.
 * @param onNeverTranslationClicked Invoked when the user clicks on the "Never Translation" button.
 * @param onDownloadLanguageClicked Invoked when the user clicks on the "Download Language" button.
 */
@Suppress("LongMethod")
@Composable
fun TranslationSettings(
    translationSwitchList: List<TranslationSwitchItem>,
    showAutomaticTranslations: Boolean,
    showNeverTranslate: Boolean,
    showDownloads: Boolean,
    pageSettingsError: TranslationError? = null,
    onAutomaticTranslationClicked: () -> Unit,
    onNeverTranslationClicked: () -> Unit,
    onDownloadLanguageClicked: () -> Unit,
) {
    val showHeader = showAutomaticTranslations || showNeverTranslate || showDownloads
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer1,
            ),
    ) {
        LazyColumn {
            items(translationSwitchList) { item: TranslationSwitchItem ->
                SwitchWithLabel(
                    checked = item.isChecked,
                    onCheckedChange = { checked ->
                        item.onStateChange.invoke(
                            item.type,
                            checked,
                        )
                    },
                    label = item.textLabel,
                    modifier = Modifier
                        .padding(start = 72.dp, end = 16.dp, top = 6.dp, bottom = 6.dp),
                )

                if (item.type.hasDivider && showHeader && pageSettingsError == null) {
                    Divider(Modifier.padding(top = 8.dp, bottom = 8.dp))
                }
            }

            if (pageSettingsError != null) {
                item {
                    TranslationPageSettingsErrorWarning()
                }
            } else {
                if (showHeader) {
                    item {
                        Text(
                            text = stringResource(
                                id = R.string.translation_settings_translation_preference,
                            ),
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(start = 72.dp, end = 16.dp, bottom = 8.dp, top = 8.dp)
                                .semantics { heading() },
                            color = FirefoxTheme.colors.textAccent,
                            style = FirefoxTheme.typography.headline8,
                        )
                    }
                }

                if (showAutomaticTranslations) {
                    item {
                        TextListItem(
                            label = stringResource(id = R.string.translation_settings_automatic_translation),
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(start = 56.dp)
                                .defaultMinSize(minHeight = 56.dp)
                                .wrapContentHeight(),
                            onClick = { onAutomaticTranslationClicked() },
                        )
                    }
                }

                if (showNeverTranslate) {
                    item {
                        TextListItem(
                            label = stringResource(
                                id = R.string.translation_settings_automatic_never_translate_sites,
                            ),
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(start = 56.dp)
                                .defaultMinSize(minHeight = 56.dp)
                                .wrapContentHeight(),
                            onClick = { onNeverTranslationClicked() },
                        )
                    }
                }

                if (showDownloads) {
                    item {
                        TextListItem(
                            label = stringResource(
                                id = R.string.translation_settings_download_language,
                            ),
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(start = 56.dp)
                                .defaultMinSize(minHeight = 56.dp)
                                .wrapContentHeight(),
                            onClick = { onDownloadLanguageClicked() },
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun TranslationPageSettingsErrorWarning() {
    val modifier = Modifier
        .fillMaxWidth()
        .padding(start = 72.dp, end = 16.dp, bottom = 16.dp, top = 16.dp)
        .defaultMinSize(minHeight = 56.dp)
        .wrapContentHeight()

    ReviewQualityCheckInfoCard(
        description = stringResource(id = R.string.translation_option_bottom_sheet_error_warning_text),
        type = ReviewQualityCheckInfoType.Warning,
        verticalRowAlignment = Alignment.CenterVertically,
        modifier = modifier,
    )
}

/**
 * Return a list of Translation option switch list item.
 */
@Composable
internal fun getTranslationSettingsSwitchList(): List<TranslationSwitchItem> {
    return mutableListOf<TranslationSwitchItem>().apply {
        add(
            TranslationSwitchItem(
                type = TranslationSettingsScreenOption.OfferToTranslate(
                    hasDivider = false,
                ),
                textLabel = stringResource(R.string.translation_settings_offer_to_translate),
                isChecked = true,
                isEnabled = true,
                onStateChange = { _, _ -> },
            ),
        )
        add(
            TranslationSwitchItem(
                type = TranslationSettingsScreenOption.AlwaysDownloadInSavingMode(
                    hasDivider = true,
                ),
                textLabel = stringResource(R.string.translation_settings_always_download),
                isChecked = false,
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
        TranslationSettings(
            translationSwitchList = getTranslationSettingsSwitchList(),
            showAutomaticTranslations = true,
            showNeverTranslate = true,
            showDownloads = true,
            onAutomaticTranslationClicked = {},
            onDownloadLanguageClicked = {},
            onNeverTranslationClicked = {},
        )
    }
}
