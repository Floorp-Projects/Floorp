/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import android.content.res.Configuration.UI_MODE_NIGHT_NO
import android.content.res.Configuration.UI_MODE_NIGHT_YES
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.selectable
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.capitalize
import androidx.compose.ui.text.intl.Locale
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Default layout of a selectable chip.
 *
 * @param text [String] displayed in this chip. Ideally should only be one word.
 * @param isSelected Whether this should be shown as selected.
 * @param onClick Callback for when the user taps this.
 */
@Composable
fun SelectableChip(
    text: String,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .selectable(isSelected) { onClick() }
            .clip(MaterialTheme.shapes.small)
            .background(
                color = if (isSelected) {
                    FirefoxTheme.colors.actionPrimary
                } else {
                    FirefoxTheme.colors.actionTertiary
                }
            )
            .padding(horizontal = 16.dp, vertical = 10.dp)
    ) {
        Text(
            text = text.capitalize(Locale.current),
            style = TextStyle(fontSize = 14.sp),
            color = if (isSelected) {
                FirefoxTheme.colors.textActionPrimary
            } else {
                FirefoxTheme.colors.textActionTertiary
            }
        )
    }
}

@Composable
@Preview(uiMode = UI_MODE_NIGHT_YES)
private fun SelectableChipDarkThemePreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(FirefoxTheme.colors.layer1),
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            SelectableChip(text = "Chirp", isSelected = false) { }
            SelectableChip(text = "Chirp", isSelected = true) { }
        }
    }
}

@Composable
@Preview(uiMode = UI_MODE_NIGHT_NO)
private fun SelectableChipLightThemePreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(FirefoxTheme.colors.layer1),
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            SelectableChip(text = "Chirp", isSelected = false) { }
            SelectableChip(text = "Chirp", isSelected = true) { }
        }
    }
}
