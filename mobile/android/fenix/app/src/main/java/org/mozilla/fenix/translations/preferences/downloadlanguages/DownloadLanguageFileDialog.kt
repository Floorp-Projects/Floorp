/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.downloadlanguages

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Checkbox
import androidx.compose.material.CheckboxDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import mozilla.components.feature.downloads.toMegabyteOrKilobyteString
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.TextButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Download Languages File Dialog.
 *
 * @param fileSize Language file size in bytes that should be displayed in the dialogue title.
 * @param isCheckBoxEnabled Whether saving mode checkbox is checked or unchecked.
 * @param isAllLanguagesItemType Whether the download language file item is of type all languages.
 * @param onSavingModeStateChange Invoked when the user clicks on the checkbox of the saving mode state.
 * @param onConfirmDownload Invoked when the user click on the "Download" dialog button.
 * @param onCancel Invoked when the user clicks on the "Cancel" dialog button.
 */
@Suppress("LongParameterList")
@Composable
fun DownloadLanguageFileDialog(
    fileSize: Long,
    isCheckBoxEnabled: Boolean,
    isAllLanguagesItemType: Boolean,
    onSavingModeStateChange: (Boolean) -> Unit,
    onConfirmDownload: () -> Unit,
    onCancel: () -> Unit,
) {
    Dialog(onDismissRequest = {}) {
        Column(
            modifier = Modifier
                .background(
                    color = FirefoxTheme.colors.layer2,
                    shape = RoundedCornerShape(8.dp),
                )
                .padding(16.dp),
        ) {
            Text(
                text = stringResource(
                    R.string.download_language_file_dialog_title,
                    fileSize.toMegabyteOrKilobyteString(),
                ),
                modifier = Modifier
                    .semantics { heading() },
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline7,
            )

            if (isAllLanguagesItemType) {
                Text(
                    text = stringResource(
                        R.string.download_language_file_dialog_message_all_languages,
                    ),
                    modifier = Modifier.padding(top = 16.dp, bottom = 16.dp),
                    style = FirefoxTheme.typography.body2,
                    color = FirefoxTheme.colors.textPrimary,
                )
            }

            DownloadLanguageFileDialogCheckbox(
                isCheckBoxEnabled = isCheckBoxEnabled,
                onSavingModeStateChange = onSavingModeStateChange,
            )

            val primaryButtonText: String = if (isAllLanguagesItemType) {
                stringResource(id = R.string.download_language_file_dialog_positive_button_text_all_languages)
            } else {
                stringResource(id = R.string.download_language_file_dialog_positive_button_text)
            }

            PrimaryButton(
                text = primaryButtonText,
                modifier = Modifier
                    .padding(top = 16.dp)
                    .fillMaxWidth(),
                onClick = {
                    onConfirmDownload()
                },
            )

            TextButton(
                text = stringResource(id = R.string.download_language_file_dialog_negative_button_text),
                modifier = Modifier
                    .fillMaxWidth(),
                onClick = {
                    onCancel()
                },
            )
        }
    }
}

@Composable
private fun DownloadLanguageFileDialogCheckbox(
    isCheckBoxEnabled: Boolean,
    onSavingModeStateChange: (Boolean) -> Unit,
) {
    val checkBoxText = stringResource(
        R.string.download_language_file_dialog_checkbox_text,
    )
    Row(
        modifier = Modifier
            .toggleable(
                value = isCheckBoxEnabled,
                role = Role.Checkbox,
                onValueChange = onSavingModeStateChange,
            )
            .defaultMinSize(minHeight = 56.dp),
    ) {
        Checkbox(
            modifier = Modifier
                .align(Alignment.CenterVertically)
                .clearAndSetSemantics { },
            checked = isCheckBoxEnabled,
            onCheckedChange = onSavingModeStateChange,
            colors = CheckboxDefaults.colors(
                checkedColor = FirefoxTheme.colors.formSelected,
                uncheckedColor = FirefoxTheme.colors.formDefault,
            ),
        )

        Spacer(modifier = Modifier.width(20.dp))

        Text(
            modifier = Modifier
                .align(Alignment.CenterVertically),
            text = checkBoxText,
            style = FirefoxTheme.typography.body2,
            color = FirefoxTheme.colors.textPrimary,
        )
    }
}

@Composable
@LightDarkPreview
private fun PrefDownloadLanguageFileDialogPreviewAllLanguages() {
    FirefoxTheme {
        DownloadLanguageFileDialog(
            fileSize = 4000L,
            isCheckBoxEnabled = true,
            isAllLanguagesItemType = true,
            onSavingModeStateChange = {},
            onConfirmDownload = {},
            onCancel = {},
        )
    }
}

@Composable
@LightDarkPreview
private fun PrefDownloadLanguageFileDialogPreview() {
    FirefoxTheme {
        DownloadLanguageFileDialog(
            fileSize = 4000L,
            isCheckBoxEnabled = false,
            isAllLanguagesItemType = false,
            onSavingModeStateChange = {},
            onConfirmDownload = {},
            onCancel = {},
        )
    }
}
