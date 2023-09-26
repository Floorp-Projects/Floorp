/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

private val cardShape = RoundedCornerShape(8.dp)
private val defaultCardElevation = 5.dp
private val defaultCardContentPadding = 16.dp

/**
 * A card container for review quality check UI that can be expanded and collapsed.
 *
 * @param title The title of the card.
 * @param modifier Modifier to be applied to the card.
 * @param onExpandToggleClick Callback invoked when card is collapsed or expanded.
 * @param content The content of the card.
 */
@Composable
fun ReviewQualityCheckExpandableCard(
    title: String,
    modifier: Modifier = Modifier,
    onExpandToggleClick: (isExpanded: Boolean) -> Unit = {},
    content: @Composable () -> Unit,
) {
    ReviewQualityCheckCard(
        modifier = modifier,
        contentPadding = PaddingValues(0.dp),
    ) {
        var isExpanded by remember { mutableStateOf(false) }

        Row(
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier
                .fillMaxWidth()
                .clickable {
                    isExpanded = isExpanded.not()
                    onExpandToggleClick(isExpanded)
                }
                .padding(defaultCardContentPadding),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = title,
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline8,
            )

            val chevronDrawable = if (isExpanded) {
                R.drawable.mozac_ic_chevron_up_20
            } else {
                R.drawable.mozac_ic_chevron_down_20
            }

            Icon(
                painter = painterResource(id = chevronDrawable),
                contentDescription = null,
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }

        AnimatedVisibility(visible = isExpanded) {
            Box(
                modifier = Modifier.padding(
                    start = defaultCardContentPadding,
                    end = defaultCardContentPadding,
                    bottom = defaultCardContentPadding,
                ),
            ) {
                content()
            }
        }
    }
}

/**
 * A card container for review quality check UI.
 *
 * @param modifier Modifier to be applied to the card.
 * @param backgroundColor The background color of the card.
 * @param elevation The elevation of the card.
 * @param contentPadding Padding used within the card container.
 * @param content The content of the card.
 */
@Composable
fun ReviewQualityCheckCard(
    modifier: Modifier,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    elevation: Dp = defaultCardElevation,
    contentPadding: PaddingValues = PaddingValues(defaultCardContentPadding),
    content: @Composable ColumnScope.() -> Unit,
) {
    Card(
        shape = cardShape,
        backgroundColor = backgroundColor,
        elevation = elevation,
        modifier = modifier,
    ) {
        Column(
            modifier = Modifier.padding(contentPadding),
        ) {
            content()
        }
    }
}

@LightDarkPreview
@Composable
private fun ReviewQualityCheckCardPreview() {
    FirefoxTheme {
        Column(modifier = Modifier.padding(16.dp)) {
            ReviewQualityCheckCard(
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(
                    text = "Review Quality Check Card Content",
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.headline8,
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            ReviewQualityCheckExpandableCard(
                title = "Review Quality Check Expandable Card",
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(
                    text = "content",
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.body2,
                )
            }
        }
    }
}
