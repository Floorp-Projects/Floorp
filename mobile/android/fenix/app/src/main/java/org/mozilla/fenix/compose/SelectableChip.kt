/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.selectable
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.capitalize
import androidx.compose.ui.text.intl.Locale
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.theme.FirefoxTheme

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
    val contentColor = when (isSystemInDarkTheme()) {
        true -> PhotonColors.LightGrey10
        false -> if (isSelected) PhotonColors.LightGrey10 else PhotonColors.DarkGrey90
    }

    @Suppress("MagicNumber")
    val backgroundColor = when (isSystemInDarkTheme()) {
        true -> if (isSelected) PhotonColors.Violet50 else PhotonColors.DarkGrey50
        // Custom color codes matching the Figma design.
        false -> if (isSelected) { Color(0xFF312A65) } else { Color(0x1420123A) }
    }

    Box(
        modifier = Modifier
            .selectable(isSelected) { onClick() }
            .clip(MaterialTheme.shapes.small)
            .background(backgroundColor)
            .padding(16.dp, 10.dp)
    ) {
        Text(
            text = text.capitalize(Locale.current),
            style = TextStyle(fontSize = 14.sp),
            color = contentColor
        )
    }
}

@Composable
@Preview
private fun SelectableChipPreview() {
    FirefoxTheme {
        Box(Modifier.fillMaxSize().background(FirefoxTheme.colors.surface)) {
            SelectableChip("Chirp", false) { }
            SelectableChip(text = "Chirp", isSelected = true) { }
        }
    }
}
