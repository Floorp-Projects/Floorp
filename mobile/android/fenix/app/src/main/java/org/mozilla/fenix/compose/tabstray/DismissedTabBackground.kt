/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.tabstray

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.DismissDirection
import androidx.compose.material.Icon
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import mozilla.components.feature.tab.collections.Tab
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The background of a [Tab] that is being swiped left or right.
 *
 * @param dismissDirection [DismissDirection] of the ongoing swipe. Depending on the direction,
 * the background will also include a warning icon at the start of the swipe gesture.
 * If `null` the warning icon will be shown at both ends.
 * @param shape Shape of the background.
 */
@Composable
fun DismissedTabBackground(
    dismissDirection: DismissDirection?,
    shape: Shape,
) {
    Card(
        modifier = Modifier.fillMaxSize(),
        backgroundColor = FirefoxTheme.colors.layer3,
        shape = shape,
        elevation = 0.dp,
    ) {
        Row(
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                painter = painterResource(R.drawable.ic_delete),
                contentDescription = null,
                modifier = Modifier
                    .padding(horizontal = 32.dp)
                    // Only show the delete icon for where the swipe starts.
                    .alpha(
                        if (dismissDirection == DismissDirection.StartToEnd || dismissDirection == null) 1f else 0f,
                    ),
                tint = FirefoxTheme.colors.iconWarning,
            )

            Icon(
                painter = painterResource(R.drawable.ic_delete),
                contentDescription = null,
                modifier = Modifier
                    .padding(horizontal = 32.dp)
                    // Only show the delete icon for where the swipe starts.
                    .alpha(
                        if (dismissDirection == DismissDirection.EndToStart || dismissDirection == null) 1f else 0f,
                    ),
                tint = FirefoxTheme.colors.iconWarning,
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun DismissedTabBackgroundPreview() {
    FirefoxTheme {
        Column {
            Box(modifier = Modifier.height(56.dp)) {
                DismissedTabBackground(
                    dismissDirection = DismissDirection.StartToEnd,
                    shape = RoundedCornerShape(0.dp),
                )
            }

            Spacer(Modifier.height(10.dp))

            Box(modifier = Modifier.height(56.dp)) {
                DismissedTabBackground(
                    dismissDirection = DismissDirection.EndToStart,
                    shape = RoundedCornerShape(bottomStart = 8.dp, bottomEnd = 8.dp),
                )
            }

            Spacer(Modifier.height(10.dp))

            Box(modifier = Modifier.height(56.dp)) {
                DismissedTabBackground(
                    dismissDirection = null,
                    shape = RoundedCornerShape(0.dp),
                )
            }
        }
    }
}
