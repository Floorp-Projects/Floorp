/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
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
fun TranslationOptionsDialog(
    translationOptionsList: List<TranslationSwitchItem>,
    onBackClicked: () -> Unit,
    onTranslationSettingsClicked: () -> Unit,
    aboutTranslationClicked: () -> Unit,
) {
    TranslationOptionsDialogHeader(onBackClicked)

    val translationOptionsListState = remember {
        translationOptionsList.toMutableStateList()
    }

    EnabledTranslationOptionsItems(
        translationOptionsListState,
    )

    LazyColumn {
        items(translationOptionsListState) { item: TranslationSwitchItem ->

            val translationSwitchItem = TranslationSwitchItem(
                textLabel = item.textLabel,
                description = if (item.isChecked) item.description else null,
                importance = item.importance,
                isChecked = item.isChecked,
                isEnabled = item.isEnabled,
                hasDivider = item.hasDivider,
                onStateChange = { checked ->
                    // If the item has the same importance, only one switch should be enabled.
                    val iterator = translationOptionsListState.iterator()
                    iterator.forEach {
                        if (it != item && it.importance == item.importance && it.isChecked) {
                            it.isChecked = false
                        }
                    }

                    val index = translationOptionsListState.indexOf(item)
                    translationOptionsListState[index] = translationOptionsListState[index].copy(
                        isChecked = checked,
                    )
                },
            )
            TranslationOptions(
                translationSwitchItem = translationSwitchItem,
            )
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
                    formatArgs = arrayOf(stringResource(R.string.firefox)),
                ),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(start = 56.dp),
                onClick = { aboutTranslationClicked() },
            )
        }
    }
}

/**
 * If the item with the highest importance is checked, all other items should be disabled.
 * If all items are unchecked, all of them are enabled.
 * If the item with the highest importance is unchecked and a item with importance 1 is checked ,
 * the item with importance 0 is disabled.
 * If the item with importance 0 is checked all the items are enabled.
 */
@Composable
private fun EnabledTranslationOptionsItems(
    translationOptionsListState: SnapshotStateList<TranslationSwitchItem>,
) {
    val itemCheckedWithHighestImportance =
        translationOptionsListState.sortedByDescending { listItem -> listItem.importance }
            .firstOrNull { it.isChecked }

    if (itemCheckedWithHighestImportance == null || itemCheckedWithHighestImportance.importance == 0) {
        translationOptionsListState.forEach {
            it.isEnabled = true
        }
    } else {
        translationOptionsListState.forEach {
            it.isEnabled = it.importance >= itemCheckedWithHighestImportance.importance
        }
    }
}

@Composable
private fun TranslationOptions(
    translationSwitchItem: TranslationSwitchItem,
) {
    SwitchWithLabel(
        label = translationSwitchItem.textLabel,
        description = translationSwitchItem.description,
        enabled = translationSwitchItem.isEnabled,
        checked = translationSwitchItem.isChecked,
        onCheckedChange = translationSwitchItem.onStateChange,
        modifier = Modifier.padding(start = 72.dp, end = 16.dp),
    )

    if (translationSwitchItem.hasDivider) {
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
                isEnabled = true,
                onStateChange = { },
            ),
        )
        add(
            TranslationSwitchItem(
                textLabel = stringResource(
                    id = R.string.translation_option_bottom_sheet_always_translate_in_language,
                    formatArgs = arrayOf(Locale("es").displayName),
                ),
                description = stringResource(id = R.string.translation_option_bottom_sheet_switch_description),
                importance = 1,
                isChecked = false,
                isEnabled = true,
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
                description = stringResource(id = R.string.translation_option_bottom_sheet_switch_description),
                importance = 1,
                isChecked = true,
                isEnabled = true,
                hasDivider = true,
                onStateChange = {},
            ),
        )
        add(
            TranslationSwitchItem(
                textLabel = stringResource(R.string.translation_option_bottom_sheet_never_translate_site),
                description = stringResource(
                    id = R.string.translation_option_bottom_sheet_switch_never_translate_site_description,
                ),
                importance = 2,
                isChecked = true,
                isEnabled = true,
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
        TranslationOptionsDialog(
            translationOptionsList = getTranslationOptionsList(),
            onBackClicked = {},
            onTranslationSettingsClicked = {},
            aboutTranslationClicked = {},
        )
    }
}
