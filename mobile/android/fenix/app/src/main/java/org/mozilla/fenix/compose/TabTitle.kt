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
import org.mozilla.fenix.theme.Theme

/**
 * Default layout for a tab composable title.
 *
 * @param text Tab title
 * @param maxLines Maximum number of lines for [text] to span, wrapping if necessary.
 * If the text exceeds the given number of lines it will be ellipsized.
 * @param modifier Optional [Modifier] to be applied to the layout.
 */
@Composable
fun TabTitle(
    text: String,
    maxLines: Int,
    modifier: Modifier = Modifier
) {
    Text(
        modifier = modifier,
        maxLines = maxLines,
        text = text,
        style = TextStyle(fontSize = 14.sp),
        overflow = TextOverflow.Ellipsis,
        color = FirefoxTheme.colors.textPrimary
    )
}

@Composable
@Preview
private fun TabTitlePreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Box(Modifier.background(FirefoxTheme.colors.layer2)) {
            TabTitle(
                "Awesome tab title",
                2
            )
        }
    }
}
