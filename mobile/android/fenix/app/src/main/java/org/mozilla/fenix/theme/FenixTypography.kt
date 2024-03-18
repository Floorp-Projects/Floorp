/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.theme

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * A custom typography for Mozilla Firefox for Android (Fenix).
 *
 * @property headline5 Currently not in-use.
 * @property headline6 Used for headings on Onboarding Modals and App Bar Titles.
 * @property headline7 Used for headings on Cards, Dialogs, Banners, and Homepage.
 * @property headline8 Used for Small Headings.
 * @property subtitle1 Used for Lists.
 * @property subtitle2 Currently not in-use.
 * @property body1 Currently not in-use.
 * @property body2 Used for body text.
 * @property button Used for Buttons.
 * @property caption Used for captions.
 * @property overline Used for Sheets.
 */
@Suppress("LongParameterList")
class FenixTypography(
    val headline5: TextStyle,
    val headline6: TextStyle,
    val headline7: TextStyle,
    val headline8: TextStyle,
    val subtitle1: TextStyle,
    val subtitle2: TextStyle,
    val body1: TextStyle,
    val body2: TextStyle,
    val button: TextStyle,
    val caption: TextStyle,
    val overline: TextStyle,
)

val defaultTypography = FenixTypography(
    headline5 = TextStyle(
        fontSize = 24.sp,
        fontWeight = FontWeight.W400,
        letterSpacing = 0.18.sp,
        lineHeight = 24.sp,
    ),

    headline6 = TextStyle(
        fontSize = 20.sp,
        fontWeight = FontWeight.W500,
        letterSpacing = 0.15.sp,
        lineHeight = 24.sp,
    ),

    headline7 = TextStyle(
        fontSize = 16.sp,
        fontWeight = FontWeight.W500,
        letterSpacing = 0.15.sp,
        lineHeight = 24.sp,
    ),

    headline8 = TextStyle(
        fontSize = 14.sp,
        fontWeight = FontWeight.W500,
        letterSpacing = 0.4.sp,
        lineHeight = 20.sp,
    ),

    subtitle1 = TextStyle(
        fontSize = 16.sp,
        fontWeight = FontWeight.W400,
        letterSpacing = 0.15.sp,
        lineHeight = 24.sp,
    ),

    subtitle2 = TextStyle(
        fontSize = 14.sp,
        fontWeight = FontWeight.W500,
        letterSpacing = 0.1.sp,
        lineHeight = 24.sp,
    ),

    body1 = TextStyle(
        fontSize = 16.sp,
        fontWeight = FontWeight.W400,
        letterSpacing = 0.5.sp,
        lineHeight = 24.sp,
    ),

    body2 = TextStyle(
        fontSize = 14.sp,
        fontWeight = FontWeight.W400,
        letterSpacing = 0.25.sp,
        lineHeight = 20.sp,
    ),

    button = TextStyle(
        fontSize = 14.sp,
        fontWeight = FontWeight.W500,
        letterSpacing = 0.25.sp,
        lineHeight = 14.sp,
    ),

    caption = TextStyle(
        fontSize = 12.sp,
        fontWeight = FontWeight.W400,
        letterSpacing = 0.4.sp,
        lineHeight = 16.sp,
    ),

    overline = TextStyle(
        fontSize = 10.sp,
        fontWeight = FontWeight.W400,
        letterSpacing = 1.5.sp,
        lineHeight = 16.sp,
    ),
)

@Composable
@Preview
private fun TypographyPreview() {
    val textStyles = listOf(
        Pair("Headline 5", defaultTypography.headline5),
        Pair("Headline 6", defaultTypography.headline6),
        Pair("Headline 7", defaultTypography.headline7),
        Pair("Headline 8", defaultTypography.headline8),
        Pair("Subtitle1", defaultTypography.subtitle1),
        Pair("Subtitle2", defaultTypography.subtitle2),
        Pair("Body1", defaultTypography.body1),
        Pair("Body2", defaultTypography.body2),
        Pair("Button", defaultTypography.button),
        Pair("Caption", defaultTypography.caption),
        Pair("Overline", defaultTypography.overline),
    )

    FirefoxTheme {
        LazyColumn(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.SpaceBetween,
        ) {
            items(textStyles) { style ->
                Text(
                    text = style.first,
                    style = style.second,
                )
            }
        }
    }
}
