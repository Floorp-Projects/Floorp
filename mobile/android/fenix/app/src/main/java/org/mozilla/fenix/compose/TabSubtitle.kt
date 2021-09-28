/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Default layout for a tab composable caption.
 *
 * @param text Tab caption.
 * @param modifier Optional [Modifier] to be applied to the layout.
 */
@Composable
fun TabSubtitle(
    text: String,
    modifier: Modifier = Modifier
) {
    Text(
        modifier = modifier,
        maxLines = 1,
        text = text,
        style = TextStyle(fontSize = 12.sp),
        overflow = TextOverflow.Ellipsis,
        color = FirefoxTheme.colors.textSecondary
    )
}

@Composable
@Preview
private fun TabSubtitlePreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.surface)) {
            TabSubtitle(
                "Awesome tab subtitle",
            )
        }
    }
}
