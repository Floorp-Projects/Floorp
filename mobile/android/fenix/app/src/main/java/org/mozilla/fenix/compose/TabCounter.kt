/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Icon
import androidx.compose.material.LocalContentAlpha
import androidx.compose.material.LocalContentColor
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.ext.toLocaleString
import org.mozilla.fenix.tabstray.TabsTrayTestTag
import org.mozilla.fenix.theme.FirefoxTheme

private const val MAX_VISIBLE_TABS = 99
private const val SO_MANY_TABS_OPEN = "∞"
private val NORMAL_TABS_BOTTOM_PADDING = 0.5.dp
private const val ONE_DIGIT_SIZE_RATIO = 0.5f
private const val TWO_DIGITS_SIZE_RATIO = 0.4f
private const val MIN_SINGLE_DIGIT = 0
private const val MAX_SINGLE_DIGIT = 9
private const val TWO_DIGIT_THRESHOLD = 10
private const val TAB_TEXT_BOTTOM_PADDING_RATIO = 4

/**
 * UI for displaying the number of opened tabs.
*
* This composable uses LocalContentColor, provided by CompositionLocalProvider,
* to set the color of its icons and text.
*
* @param tabCount the number to be displayed inside the counter.
*/

@Composable
fun TabCounter(tabCount: Int) {
    val formattedTabCount = tabCount.toLocaleString()
    val normalTabCountText: String
    val tabCountTextRatio: Float
    val needsBottomPaddingForInfiniteTabs: Boolean

    when (tabCount) {
        in MIN_SINGLE_DIGIT..MAX_SINGLE_DIGIT -> {
            normalTabCountText = formattedTabCount
            tabCountTextRatio = ONE_DIGIT_SIZE_RATIO
            needsBottomPaddingForInfiniteTabs = false
        }

        in TWO_DIGIT_THRESHOLD..MAX_VISIBLE_TABS -> {
            normalTabCountText = formattedTabCount
            tabCountTextRatio = TWO_DIGITS_SIZE_RATIO
            needsBottomPaddingForInfiniteTabs = false
        }

        else -> {
            normalTabCountText = SO_MANY_TABS_OPEN
            tabCountTextRatio = ONE_DIGIT_SIZE_RATIO
            needsBottomPaddingForInfiniteTabs = true
        }
    }

    val normalTabsContentDescription = if (tabCount == 1) {
        stringResource(id = R.string.mozac_tab_counter_open_tab_tray_single)
    } else {
        stringResource(
            id = R.string.mozac_tab_counter_open_tab_tray_plural,
            formattedTabCount,
        )
    }

    val counterBoxWidthDp =
        dimensionResource(id = mozilla.components.ui.tabcounter.R.dimen.mozac_tab_counter_box_width_height)
    val counterBoxWidthPx = LocalDensity.current.run { counterBoxWidthDp.roundToPx() }
    val counterTabsTextSize = (tabCountTextRatio * counterBoxWidthPx).toInt()

    val normalTabsTextModifier = if (needsBottomPaddingForInfiniteTabs) {
        val bottomPadding = with(LocalDensity.current) { counterTabsTextSize.toDp() / TAB_TEXT_BOTTOM_PADDING_RATIO }
        Modifier.padding(bottom = bottomPadding)
    } else {
        Modifier.padding(bottom = NORMAL_TABS_BOTTOM_PADDING)
    }

    Box(
        modifier = Modifier
            .semantics(mergeDescendants = true) {
                testTag = TabsTrayTestTag.normalTabsCounter
            },
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            painter = painterResource(
                id = mozilla.components.ui.tabcounter.R.drawable.mozac_ui_tabcounter_box,
            ),
            contentDescription = normalTabsContentDescription,
        )

        Text(
            text = normalTabCountText,
            modifier = normalTabsTextModifier,
            color = LocalContentColor.current.copy(alpha = LocalContentAlpha.current),
            fontSize = with(LocalDensity.current) { counterTabsTextSize.toDp().toSp() },
            fontWeight = FontWeight.W700,
            textAlign = TextAlign.Center,
        )
    }
}

@LightDarkPreview
@Preview(locale = "ar")
@Composable
private fun TabCounterPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
        ) {
            TabCounter(tabCount = 55)
        }
    }
}
