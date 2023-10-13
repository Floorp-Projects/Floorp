/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Divider
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Firefox Translations bottom sheet dialog.
 */
@Composable
fun TranslationsDialogBottomSheet(onSettingClicked: () -> Unit) {
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer2,
                shape = RoundedCornerShape(topStart = 8.dp, topEnd = 8.dp),
            )
            .padding(16.dp),
    ) {
        TranslationsDialogHeader(onSettingClicked)

        Spacer(modifier = Modifier.height(14.dp))

        Column {
            TranslationsDropdown(
                header = stringResource(id = R.string.translations_bottom_sheet_translate_from),
            )

            Spacer(modifier = Modifier.height(16.dp))

            TranslationsDropdown(
                header = stringResource(id = R.string.translations_bottom_sheet_translate_to),
            )

            Spacer(modifier = Modifier.height(16.dp))

            TranslationsDialogActionButtons()
        }
    }
}

@Composable
private fun TranslationsDialogHeader(onSettingClicked: () -> Unit) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = stringResource(id = R.string.translations_bottom_sheet_title),
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
private fun TranslationsDropdown(
    header: String,
) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(
            text = header,
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.caption,
        )

        Row {
            Text(
                text = "English",
                modifier = Modifier.weight(1f),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.subtitle1,
            )

            Spacer(modifier = Modifier.width(10.dp))

            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_dropdown_arrow),
                contentDescription = null,
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }

        Divider(color = FirefoxTheme.colors.formDefault)
    }
}

@Composable
private fun TranslationsDialogActionButtons() {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.End,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SecondaryButton(
            text = stringResource(id = R.string.translations_bottom_sheet_negative_button),
            modifier = Modifier,
            backgroundColor = Color.Transparent,
        ) {}

        Spacer(modifier = Modifier.width(10.dp))

        PrimaryButton(
            text = stringResource(id = R.string.translations_bottom_sheet_positive_button),
            modifier = Modifier,
        ) {}
    }
}

@Composable
@LightDarkPreview
private fun TranslationsDialogBottomSheetPreview() {
    FirefoxTheme {
        TranslationsDialogBottomSheet(onSettingClicked = {})
    }
}
