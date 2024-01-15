/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Translation Settings Fragment.
 *
 * @param translationSwitchList list of [TranslationSwitchItem]s to display.
 * @param onAutomaticTranslationClicked Invoked when the user clicks on the "Automatic Translation" button.
 * @param onNeverTranslationClicked Invoked when the user clicks on the "Never Translation" button.
 * @param onDownloadLanguageClicked Invoked when the user clicks on the "Download Language" button.
 */
@Composable
fun TranslationSettings(
    translationSwitchList: List<TranslationSwitchItem>,
    onAutomaticTranslationClicked: () -> Unit,
    onNeverTranslationClicked: () -> Unit,
    onDownloadLanguageClicked: () -> Unit,
) {
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
                    onCheckedChange = item.onStateChange,
                    label = item.textLabel,
                    modifier = Modifier
                        .padding(start = 72.dp, end = 16.dp),
                )

                if (item.hasDivider) {
                    Divider(Modifier.padding(top = 8.dp, bottom = 8.dp))
                }
            }

            item {
                Text(
                    text = stringResource(
                        id = R.string.translation_settings_translation_preference,
                    ),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 72.dp, end = 16.dp, bottom = 16.dp, top = 8.dp)
                        .semantics { heading() },
                    color = FirefoxTheme.colors.textAccent,
                    style = FirefoxTheme.typography.headline8,
                )
            }

            item {
                TextListItem(
                    label = stringResource(id = R.string.translation_settings_automatic_translation),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 56.dp),
                    onClick = { onAutomaticTranslationClicked() },
                )
            }

            item {
                TextListItem(
                    label = stringResource(
                        id = R.string.translation_settings_automatic_never_translate_sites,
                    ),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 56.dp),
                    onClick = { onNeverTranslationClicked() },
                )
            }

            item {
                TextListItem(
                    label = stringResource(
                        id = R.string.translation_settings_download_language,
                    ),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 56.dp),
                    onClick = { onDownloadLanguageClicked() },
                )
            }
        }
    }
}

/**
 * Return a list of Translation option switch list item.
 */
@Composable
internal fun getTranslationSettingsSwitchList(): List<TranslationSwitchItem> {
    return mutableListOf<TranslationSwitchItem>().apply {
        add(
            TranslationSwitchItem(
                textLabel = stringResource(R.string.translation_settings_offer_to_translate),
                isChecked = true,
                hasDivider = false,
                isEnabled = true,
                onStateChange = {},
            ),
        )
        add(
            TranslationSwitchItem(
                textLabel = stringResource(R.string.translation_settings_always_download),
                isChecked = false,
                hasDivider = true,
                isEnabled = true,
                onStateChange = {},
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
            onAutomaticTranslationClicked = {},
            onDownloadLanguageClicked = {},
            onNeverTranslationClicked = {},
        )
    }
}
