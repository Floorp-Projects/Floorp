/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.button

import androidx.compose.animation.animateContentSize
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.material.FloatingActionButton
import androidx.compose.material.FloatingActionButtonDefaults
import androidx.compose.material.FloatingActionButtonElevation
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Floating action button.
 *
 * @param icon [Painter] icon to be displayed inside the action button.
 * @param modifier [Modifier] to be applied to the action button.
 * @param contentDescription The content description to describe the icon.
 * @param label Text to be displayed next to the icon.
 * @param elevation [FloatingActionButtonElevation] used to resolve the elevation for this FAB in different states.
 * This controls the size of the shadow below the FAB.
 * @param onClick Invoked when the button is clicked.
 */
@Composable
fun FloatingActionButton(
    icon: Painter,
    modifier: Modifier = Modifier,
    contentDescription: String? = null,
    label: String? = null,
    elevation: FloatingActionButtonElevation = FloatingActionButtonDefaults.elevation(
        defaultElevation = 5.dp,
        pressedElevation = 5.dp,
    ),
    onClick: () -> Unit,
) {
    FloatingActionButton(
        onClick = onClick,
        modifier = modifier,
        backgroundColor = FirefoxTheme.colors.actionPrimary,
        contentColor = FirefoxTheme.colors.textActionPrimary,
        elevation = elevation,
    ) {
        Row(
            modifier = Modifier
                .wrapContentSize()
                .padding(16.dp)
                .animateContentSize(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                painter = icon,
                contentDescription = contentDescription,
                tint = FirefoxTheme.colors.iconOnColor,
            )

            if (!label.isNullOrBlank()) {
                Spacer(Modifier.width(12.dp))

                Text(
                    text = label,
                    style = FirefoxTheme.typography.button,
                    maxLines = 1,
                )
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun FloatingActionButtonPreview() {
    var label by remember { mutableStateOf<String?>("LABEL") }

    FirefoxTheme {
        Box(Modifier.wrapContentSize()) {
            FloatingActionButton(
                label = label,
                icon = painterResource(R.drawable.ic_new),
                onClick = {
                    label = if (label == null) "LABEL" else null
                },
            )
        }
    }
}
