/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.Layout
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Special caption text for a tab layout shown on one line.
 *
 * This will combine [firstText] with a interdot and then [secondText] ensuring that the second text
 * (which is assumed to be smaller) always fills as much space as needed with the [firstText] automatically
 * being resized to be smaller with an added ellipsis characters if needed.
 *
 * Possible results:
 * ```
 * - when both texts would fit the screen
 * ------------------------------------------
 * |firstText · secondText                  |
 * ------------------------------------------
 *
 * - when both text do not fit, second is shown in entirety, first is ellipsised.
 * ------------------------------------------
 * |longerFirstTextOrSmallSc... · secondText|
 * ------------------------------------------
 * ```
 *
 * @param firstText Text shown at the start of the row.
 * @param secondText Text shown at the end of the row.
 */
@Composable
fun TabSubtitleWithInterdot(
    firstText: String,
    secondText: String,
) {
    val currentLayoutDirection = LocalLayoutDirection.current

    Layout(
        content = {
            Text(
                text = firstText,
                color = FirefoxTheme.colors.textSecondary,
                fontSize = 12.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )
            Text(
                text = " \u00b7 ",
                color = FirefoxTheme.colors.textSecondary,
                fontSize = 12.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )
            Text(
                text = secondText,
                color = FirefoxTheme.colors.textSecondary,
                fontSize = 12.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )
        }
    ) { items, constraints ->

        // We need to measure from the end to start to ensure the secondItem will always be on screen
        // and depending on secondItem's width and interdot's width the firstItem is automatically resized.
        val secondItem = items[2].measure(constraints)
        val interdot = items[1].measure(
            constraints.copy(maxWidth = constraints.maxWidth - secondItem.width)
        )
        val firstItem = items[0].measure(
            constraints.copy(maxWidth = constraints.maxWidth - secondItem.width - interdot.width)
        )

        layout(constraints.maxWidth, constraints.maxHeight) {
            val itemsPositions = IntArray(items.size)
            with(Arrangement.Start) {
                arrange(
                    constraints.maxWidth,
                    intArrayOf(firstItem.width, interdot.width, secondItem.width),
                    currentLayoutDirection,
                    itemsPositions
                )
            }

            val placementHeight = constraints.maxHeight - firstItem.height
            listOf(firstItem, interdot, secondItem).forEachIndexed { index, item ->
                item.place(itemsPositions[index], placementHeight)
            }
        }
    }
}

@Composable
@Preview
private fun TabSubtitleWithInterdotPreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Box(Modifier.background(FirefoxTheme.colors.layer2)) {
            TabSubtitleWithInterdot(
                firstText = "firstText",
                secondText = "secondText",
            )
        }
    }
}
