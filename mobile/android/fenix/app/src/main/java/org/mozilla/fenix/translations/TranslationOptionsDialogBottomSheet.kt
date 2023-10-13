/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.rememberNestedScrollInteropConnection
import androidx.compose.ui.res.painterResource
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
import java.util.Locale

/**
 * Firefox Translation options bottom sheet dialog.
 *
 * @param translationOptionsList A list of [TranslationSwitchItem]s to display.
 * @param onBackClicked Invoked when the user clicks on the back button.
 * @param onTranslationSettingsClicked Invoked when the user clicks on the "Translation Settings" button.
 * @param aboutTranslationClicked Invoked when the user clicks on the "About Translation" button.
 */
@Composable
fun TranslationOptionsDialogBottomSheet(
    translationOptionsList: List<TranslationSwitchItem>,
    onBackClicked: () -> Unit,
    onTranslationSettingsClicked: () -> Unit,
    aboutTranslationClicked: () -> Unit,
) {
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer2,
                shape = RoundedCornerShape(topStart = 8.dp, topEnd = 8.dp),
            )
            .nestedScroll(rememberNestedScrollInteropConnection()),
    ) {
        TranslationOptionsDialogHeader(onBackClicked)

        LazyColumn {
            items(translationOptionsList) { item: TranslationSwitchItem ->
                SwitchWithLabel(
                    checked = item.isChecked,
                    onCheckedChange = item.onStateChange,
                    label = item.textLabel,
                    modifier = Modifier
                        .padding(start = 72.dp, end = 16.dp),
                )

                if (item.hasDivider) {
                    Divider(Modifier.padding(top = 4.dp, bottom = 4.dp))
                }
            }

            item {
                TextListItem(
                    label = stringResource(id = R.string.translation_option_bottom_sheet_translation_settings),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 56.dp),
                    onClick = { onTranslationSettingsClicked() },
                )
            }

            item {
                TextListItem(
                    label = stringResource(
                        id = R.string.translation_option_bottom_sheet_about_translations,
                        formatArgs = arrayOf(stringResource(R.string.app_name)),
                    ),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 56.dp),
                    onClick = { aboutTranslationClicked() },
                )
            }
        }
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
            modifier = Modifier
                .size(24.dp),
        ) {
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_back_24),
                contentDescription = stringResource(R.string.etp_back_button_content_description),
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }

        Spacer(modifier = Modifier.width(32.dp))

        Text(
            text = stringResource(id = R.string.translation_option_bottom_sheet_title),
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
                textLabel = stringResource(R.string.translation_option_bottom_sheet_always_translate),
                isChecked = false,
                hasDivider = true,
                onStateChange = {},
            ),
        )
        add(
            TranslationSwitchItem(
                textLabel = stringResource(
                    id = R.string.translation_option_bottom_sheet_always_translate_in_language,
                    formatArgs = arrayOf(Locale("es").displayName),
                ),
                isChecked = false,
                hasDivider = false,
                onStateChange = {},
            ),
        )
        add(
            TranslationSwitchItem(
                textLabel = stringResource(
                    id = R.string.translation_option_bottom_sheet_never_translate_in_language,
                    formatArgs = arrayOf(Locale("es").displayName),
                ),
                isChecked = true,
                hasDivider = true,
                onStateChange = {},
            ),
        )
        add(
            TranslationSwitchItem(
                textLabel = stringResource(R.string.translation_option_bottom_sheet_never_translate_site),
                isChecked = true,
                hasDivider = true,
                onStateChange = {},
            ),
        )
    }
}

@Composable
@LightDarkPreview
private fun TranslationSettingsPreview() {
    FirefoxTheme {
        TranslationOptionsDialogBottomSheet(
            translationOptionsList = getTranslationOptionsList(),
            onBackClicked = {},
            onTranslationSettingsClicked = {},
            aboutTranslationClicked = {},
        )
    }
}
