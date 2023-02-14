/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.Layout
import androidx.compose.ui.layout.Placeable
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Displays a list of items as a staggered horizontal grid placing them on ltr rows and continuing
 * on as many below rows as needed to place all items.
 *
 * In an effort to best utilize the available row space this can mix the items such that narrower ones
 * are placed on the same row as wider ones if the otherwise next item doesn't fit.
 *
 * @param modifier [Modifier] to be applied to the layout.
 * @param horizontalItemsSpacing Minimum horizontal space between items. Does not add spacing to layout bounds.
 * @param verticalItemsSpacing Vertical space between items
 * @param arrangement How the items will be horizontally aligned and spaced.
 * @param content The children composables to be laid out.
 */
@Composable
fun StaggeredHorizontalGrid(
    modifier: Modifier = Modifier,
    horizontalItemsSpacing: Dp = 0.dp,
    verticalItemsSpacing: Dp = 8.dp,
    arrangement: Arrangement.Horizontal = Arrangement.Start,
    content: @Composable () -> Unit,
) {
    val currentLayoutDirection = LocalLayoutDirection.current

    Layout(content, modifier) { items, constraints ->
        val horizontalItemsSpacingPixels = horizontalItemsSpacing.roundToPx()
        val verticalItemsSpacingPixels = verticalItemsSpacing.roundToPx()
        var totalHeight = 0
        val itemsRows = mutableListOf<List<Placeable>>()
        val notYetPlacedItems = items.map {
            it.measure(constraints)
        }.toMutableList()

        fun getIndexOfNextPlaceableThatFitsRow(available: List<Placeable>, currentWidth: Int): Int {
            return available.indexOfFirst {
                currentWidth + it.width <= constraints.maxWidth
            }
        }

        // Populate each row with as many items as possible combining wider with narrower items.
        // This will change the order of shown categories.
        var (currentRow, currentWidth) = mutableListOf<Placeable>() to 0
        while (notYetPlacedItems.isNotEmpty()) {
            if (currentRow.isEmpty()) {
                currentRow.add(
                    notYetPlacedItems[0].also {
                        currentWidth += it.width + horizontalItemsSpacingPixels
                        totalHeight += it.height + verticalItemsSpacingPixels
                    },
                )
                notYetPlacedItems.removeAt(0)
            } else {
                val nextPlaceableThatFitsIndex = getIndexOfNextPlaceableThatFitsRow(notYetPlacedItems, currentWidth)
                if (nextPlaceableThatFitsIndex >= 0) {
                    currentRow.add(
                        notYetPlacedItems[nextPlaceableThatFitsIndex].also {
                            currentWidth += it.width + horizontalItemsSpacingPixels
                        },
                    )
                    notYetPlacedItems.removeAt(nextPlaceableThatFitsIndex)
                } else {
                    itemsRows.add(currentRow)
                    currentRow = mutableListOf()
                    currentWidth = 0
                }
            }
        }
        if (currentRow.isNotEmpty()) {
            itemsRows.add(currentRow)
        }
        totalHeight -= verticalItemsSpacingPixels

        // Place each item from each row on screen.
        layout(constraints.maxWidth, totalHeight) {
            itemsRows.forEachIndexed { rowIndex, itemRow ->
                val itemsSizes = IntArray(itemRow.size) {
                    itemRow[it].width + when (currentLayoutDirection == LayoutDirection.Ltr) {
                        true -> if (it < itemRow.lastIndex) horizontalItemsSpacingPixels else 0
                        false -> if (it > 0) horizontalItemsSpacingPixels else 0
                    }
                }
                val itemsPositions = IntArray(itemsSizes.size) { 0 }
                with(arrangement) {
                    arrange(constraints.maxWidth, itemsSizes, currentLayoutDirection, itemsPositions)
                }

                itemRow.forEachIndexed { itemIndex, item ->
                    item.place(
                        x = itemsPositions[itemIndex],
                        y = (rowIndex * item.height) + (rowIndex * verticalItemsSpacingPixels),
                    )
                }
            }
        }
    }
}

@Composable
@Preview
private fun StaggeredHorizontalGridPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer2)) {
            StaggeredHorizontalGrid(
                horizontalItemsSpacing = 8.dp,
                arrangement = Arrangement.Center,
            ) {
                "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor"
                    .split(" ")
                    .forEach {
                        Text(text = it, color = Color.Red, modifier = Modifier.border(3.dp, Color.Blue))
                    }
            }
        }
    }
}
