/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.ChipDefaults.chipColors
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Default layout for a clickable chip.
 *
 * @param text [String] displayed in this chip.
 * @param modifier [Modifier] used to be applied to the layout of the chip.
 * @param isSquare Optional [Boolean] to control whether the Chip's corners are square or rounded.
 * @param backgroundColor Optional background [Color] for the chip.
 * @param textColor Optional text [Color] for the chip.
 * @param onClick Callback for when the user taps this chip.
 */
@OptIn(ExperimentalMaterialApi::class)
@Composable
private fun Chip(
    text: String,
    modifier: Modifier = Modifier,
    isSquare: Boolean = false,
    backgroundColor: Color = FirefoxTheme.colors.actionPrimary,
    textColor: Color = FirefoxTheme.colors.textActionPrimary,
    onClick: () -> Unit,
) {
    androidx.compose.material.Chip(
        onClick = onClick,
        modifier = modifier,
        shape = if (isSquare) RoundedCornerShape(4.dp) else RoundedCornerShape(25.dp),
        colors = chipColors(
            backgroundColor = backgroundColor,
        ),
    ) {
        Text(
            text = text,
            color = textColor,
            style = FirefoxTheme.typography.body2,
        )
    }
}

/**
 * Default layout of a selectable chip.
 *
 * @param text [String] displayed in this chip.
 * @param isSelected Whether this should be shown as selected.
 * @param isSquare Optional [Boolean] to control whether the Chip's corners are square or rounded.
 * @param selectableChipColors The color set defined by [SelectableChipColors] used to style the chip.
 * @param onClick Callback for when the user taps this chip.
 */
@Composable
fun SelectableChip(
    text: String,
    isSelected: Boolean,
    isSquare: Boolean = false,
    selectableChipColors: SelectableChipColors = SelectableChipColors.buildColors(),
    onClick: () -> Unit,
) {
    var selected by remember { mutableStateOf(isSelected) }

    Chip(
        text = text,
        backgroundColor = if (selected) {
            selectableChipColors.selectedBackgroundColor
        } else {
            selectableChipColors.unselectedBackgroundColor
        },
        isSquare = isSquare,
        textColor = if (selected) selectableChipColors.selectedTextColor else selectableChipColors.unselectedTextColor,
    ) {
        selected = !selected
        onClick()
    }
}

/**
 * Wrapper for the color parameters of [SelectableChip].
 *
 * @param selectedBackgroundColor Background [Color] when the chip is selected.
 * @param unselectedBackgroundColor Background [Color] when the chip is not selected.
 * @param selectedTextColor Text [Color] when the chip is selected.
 * @param unselectedTextColor Text [Color] when the chip is not selected.
 */
data class SelectableChipColors(
    val selectedBackgroundColor: Color,
    val unselectedBackgroundColor: Color,
    val selectedTextColor: Color,
    val unselectedTextColor: Color,
) {
    companion object {

        /**
         * Builder function used to construct an instance of [SelectableChipColors].
         */
        @Composable
        fun buildColors(
            selectedBackgroundColor: Color = FirefoxTheme.colors.actionPrimary,
            unselectedBackgroundColor: Color = FirefoxTheme.colors.actionTertiary,
            selectedTextColor: Color = FirefoxTheme.colors.textActionPrimary,
            unselectedTextColor: Color = FirefoxTheme.colors.textActionTertiary,
        ) = SelectableChipColors(
            selectedBackgroundColor = selectedBackgroundColor,
            unselectedBackgroundColor = unselectedBackgroundColor,
            selectedTextColor = selectedTextColor,
            unselectedTextColor = unselectedTextColor,
        )
    }
}

@Composable
@LightDarkPreview
private fun ChipPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1),
            verticalArrangement = Arrangement.SpaceEvenly,
        ) {
            Chip(
                text = "Chirp",
                onClick = {},
            )

            Chip(
                text = "SquareChip",
                isSquare = true,
                onClick = {},
            )
        }
    }
}

@Composable
@Preview(name = "Selectable Chip", uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(name = "Selectable Chip", uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun SelectableChipPreview() {
    FirefoxTheme {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(FirefoxTheme.colors.layer1),
            horizontalArrangement = Arrangement.SpaceEvenly,
        ) {
            SelectableChip(text = "ChirpOne", isSelected = false) {}
            SelectableChip(text = "ChirpTwo", isSelected = true, isSquare = true) {}
        }
    }
}

@Composable
@Preview(name = "Custom Colors", uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(name = "Custom Colors", uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun SelectableChipWithCustomColorsPreview() {
    FirefoxTheme {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(FirefoxTheme.colors.layer1),
            horizontalArrangement = Arrangement.SpaceEvenly,
        ) {
            SelectableChip(
                text = "Yellow",
                isSelected = false,
                selectableChipColors = SelectableChipColors(
                    selectedBackgroundColor = Color.Yellow,
                    unselectedBackgroundColor = Color.DarkGray,
                    selectedTextColor = Color.Black,
                    unselectedTextColor = Color.Gray,
                ),
            ) {}

            SelectableChip(
                text = "Cyan",
                isSelected = true,
                selectableChipColors = SelectableChipColors(
                    selectedBackgroundColor = Color.Cyan,
                    unselectedBackgroundColor = Color.DarkGray,
                    selectedTextColor = Color.Red,
                    unselectedTextColor = Color.Gray,
                ),
            ) {}
        }
    }
}
