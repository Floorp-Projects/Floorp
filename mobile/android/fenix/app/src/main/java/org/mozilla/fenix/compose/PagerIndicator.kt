/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:OptIn(ExperimentalFoundationApi::class)

package org.mozilla.fenix.compose

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * An horizontally laid out indicator for a [HorizontalPager] with the ability to leave the trail of
 * indicators to show progress, instead of just showing the current one as active.
 *
 * @param pagerState The state object of your [HorizontalPager] to be used to observe the list's state.
 * @param modifier The modifier to apply to this layout.
 * @param pageCount The size of indicators should be displayed, defaults to [PagerState.pageCount].
 * If you are implementing a looping pager with a much larger [PagerState.pageCount]
 * than indicators should displayed, e.g. [Int.MAX_VALUE], specify you real size in this param.
 * @param activeColor The color of the active page indicator, and the color of previous page
 * indicators in case [leaveTrail] is set to true.
 * @param inactiveColor The color of page indicators that are inactive.
 * @param leaveTrail Whether to leave the trail of indicators to show progress.
 * This defaults to false and just shows the current one as active.
 */
@Composable
fun PagerIndicator(
    pagerState: PagerState,
    modifier: Modifier = Modifier,
    pageCount: Int,
    activeColor: Color,
    inactiveColor: Color,
    leaveTrail: Boolean = false,
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val showActiveModifier: (pageIndex: Int) -> Boolean =
            if (leaveTrail) {
                { it <= pagerState.currentPage }
            } else {
                { it == pagerState.currentPage }
            }

        repeat(pageCount) {
            Box(
                modifier = Modifier
                    .size(6.dp)
                    .background(
                        shape = CircleShape,
                        color = if (showActiveModifier(it)) {
                            activeColor
                        } else {
                            inactiveColor
                        },
                    ),
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun PagerIndicatorPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(32.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                text = "Without trail",
                style = FirefoxTheme.typography.caption,
                color = FirefoxTheme.colors.textPrimary,
            )

            Spacer(modifier = Modifier.height(8.dp))

            PagerIndicator(
                pagerState = rememberPagerState(1),
                pageCount = 3,
                activeColor = FirefoxTheme.colors.actionPrimary,
                inactiveColor = FirefoxTheme.colors.actionSecondary,
                modifier = Modifier.align(Alignment.CenterHorizontally),
            )

            Spacer(modifier = Modifier.height(16.dp))

            Text(
                text = "With trail",
                style = FirefoxTheme.typography.caption,
                color = FirefoxTheme.colors.textPrimary,
            )

            Spacer(modifier = Modifier.height(8.dp))

            PagerIndicator(
                pagerState = rememberPagerState(1),
                pageCount = 3,
                activeColor = FirefoxTheme.colors.actionPrimary,
                inactiveColor = FirefoxTheme.colors.actionSecondary,
                leaveTrail = true,
                modifier = Modifier.align(Alignment.CenterHorizontally),
            )
        }
    }
}
