/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.material.DismissDirection
import androidx.compose.material.DismissDirection.EndToStart
import androidx.compose.material.DismissDirection.StartToEnd
import androidx.compose.material.DismissState
import androidx.compose.material.DismissValue
import androidx.compose.material.DismissValue.Default
import androidx.compose.material.DismissValue.DismissedToEnd
import androidx.compose.material.DismissValue.DismissedToStart
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.FixedThreshold
import androidx.compose.material.FractionalThreshold
import androidx.compose.material.Text
import androidx.compose.material.ThresholdConfig
import androidx.compose.material.rememberDismissState
import androidx.compose.material.swipeable
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.roundToInt

/**
 * A composable that can be dismissed by swiping left or right
 *
 * @param state The state of this component.
 * @param modifier Optional [Modifier] for this component.
 * @param enabled [Boolean] controlling whether the content is swipeable or not.
 * @param directions The set of directions in which the component can be dismissed.
 * @param dismissThreshold The threshold the item needs to be swiped in order to be dismissed.
 * @param backgroundContent A composable that is stacked behind the primary content and is exposed
 * when the content is swiped. You can/should use the [state] to have different backgrounds on each side.
 * @param dismissContent The content that can be dismissed.
 */
@Composable
@ExperimentalMaterialApi
fun SwipeToDismiss(
    state: DismissState,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    directions: Set<DismissDirection> = setOf(EndToStart, StartToEnd),
    dismissThreshold: ThresholdConfig = FractionalThreshold(DISMISS_THRESHOLD),
    backgroundContent: @Composable RowScope.() -> Unit,
    dismissContent: @Composable RowScope.() -> Unit,
) {
    val swipeWidth = with(LocalDensity.current) {
        LocalConfiguration.current.screenWidthDp.dp.toPx()
    }
    val anchors = mutableMapOf(0f to Default)
    val thresholds = { _: DismissValue, _: DismissValue ->
        dismissThreshold
    }

    if (StartToEnd in directions) anchors += swipeWidth to DismissedToEnd
    if (EndToStart in directions) anchors += -swipeWidth to DismissedToStart

    Box(
        Modifier
            .swipeable(
                state = state,
                anchors = anchors,
                thresholds = thresholds,
                orientation = Orientation.Horizontal,
                enabled = state.currentValue == Default && enabled,
                reverseDirection = LocalLayoutDirection.current == LayoutDirection.Rtl,
                resistance = null,
            )
            .then(modifier),
    ) {
        Row(
            content = backgroundContent,
            modifier = Modifier.matchParentSize(),
        )

        Row(
            content = dismissContent,
            modifier = Modifier.offset { IntOffset(state.offset.value.roundToInt(), 0) },
        )
    }
}

private const val DISMISS_THRESHOLD = 0.5f

@OptIn(ExperimentalMaterialApi::class)
@Composable
private fun SwipeablePreview(directions: Set<DismissDirection>, text: String, threshold: ThresholdConfig) {
    val state = rememberDismissState()

    Box(
        modifier = Modifier
            .height(30.dp)
            .fillMaxWidth(),
    ) {
        SwipeToDismiss(
            state = state,
            directions = directions,
            dismissThreshold = threshold,
            backgroundContent = {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(FirefoxTheme.colors.layerAccent),
                )
            },
        ) {
            Row(
                modifier = Modifier
                    .fillMaxSize()
                    .background(FirefoxTheme.colors.layer1),
                horizontalArrangement = Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(text)
            }
        }
    }
}

@Suppress("MagicNumber")
@OptIn(ExperimentalMaterialApi::class)
@Composable
@Preview
private fun SwipeToDismissPreview() {
    FirefoxTheme {
        Column {
            SwipeablePreview(
                directions = setOf(StartToEnd),
                text = "Swipe to right 50% ->",
                FractionalThreshold(.5f),
            )
            Spacer(Modifier.height(30.dp))
            SwipeablePreview(
                directions = setOf(EndToStart),
                text = "<- Swipe to left 100%",
                FractionalThreshold(1f),
            )
            Spacer(Modifier.height(30.dp))
            SwipeablePreview(
                directions = setOf(StartToEnd, EndToStart),
                text = "<- Swipe both ways 20dp ->",
                FixedThreshold(20.dp),
            )
        }
    }
}
