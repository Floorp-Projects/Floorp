/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Default layout for the header of a screen section.
 *
 * @param text [String] to be styled as header and displayed.
 * @param modifier [Modifier] to be applied to the [Text].
 */
@Composable
fun SectionHeader(
    text: String,
    modifier: Modifier = Modifier
) {
    Text(
        modifier = modifier,
        text = text,
        style = TextStyle(
            fontFamily = FontFamily(Font(R.font.metropolis_semibold)),
            fontSize = 20.sp,
            lineHeight = 20.sp
        ),
        maxLines = 1,
        overflow = TextOverflow.Ellipsis,
        color = FirefoxTheme.colors.textPrimary
    )
}

/**
 * Default layout for the header of a screen section.
 *
 * @param text [String] to be styled as header and displayed.
 * @param modifier [Modifier] to be applied to the [Text].
 */
@Composable
fun HomeSectionHeader(
    text: String,
    modifier: Modifier = Modifier
) {
    Text(
        modifier = modifier,
        text = text,
        style = TextStyle(
            fontFamily = FontFamily(Font(R.font.metropolis_semibold)),
            fontSize = 16.sp,
            lineHeight = 20.sp
        ),
        maxLines = 1,
        overflow = TextOverflow.Ellipsis,
        color = FirefoxTheme.colors.textPrimary
    )
}

@Composable
@Preview
private fun HeadingTextPreview() {
    SectionHeader(text = "Section title")
}

@Composable
@Preview
private fun HomeHeadingTextPreview() {
    HomeSectionHeader(text = "Home section title")
}
