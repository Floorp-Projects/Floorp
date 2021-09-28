/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.text.ClickableText
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.tooling.preview.Preview
import mozilla.components.ui.colors.PhotonColors

/**
 * [Text] containing a substring styled as an URL informing when this is clicked.
 *
 * @param text Full text that will be displayed
 * @param textColor [Color] of the normal text. The URL substring will have a default URL style applied.
 * @param clickableStartIndex [text] index at which the URL substring starts.
 * @param clickableEndIndex [text] index at which the URL substring ends.
 * @param onClick Callback to be invoked only when the URL substring is clicked.
 */
@Composable
fun ClickableSubstringLink(
    text: String,
    textColor: Color,
    clickableStartIndex: Int,
    clickableEndIndex: Int,
    onClick: () -> Unit
) {
    val annotatedText = buildAnnotatedString {
        append(text)

        addStyle(
            SpanStyle(textColor),
            start = 0,
            end = clickableStartIndex
        )

        addStyle(
            SpanStyle(
                textDecoration = TextDecoration.Underline,
                color = when (isSystemInDarkTheme()) {
                    true -> PhotonColors.Violet40
                    false -> PhotonColors.Violet70
                }
            ),
            start = clickableStartIndex,
            end = clickableEndIndex
        )

        addStyle(
            SpanStyle(textColor),
            start = clickableEndIndex,
            end = text.length
        )

        addStringAnnotation(
            tag = "link",
            annotation = "",
            start = clickableStartIndex,
            end = clickableEndIndex
        )
    }

    ClickableText(
        text = annotatedText,
        onClick = {
            annotatedText
                .getStringAnnotations("link", it, it)
                .firstOrNull()?.let {
                    onClick()
                }
        }
    )
}

@Composable
@Preview
private fun ClickableSubstringTextPreview() {
    val text = "This text contains a link"
    Box(modifier = Modifier.background(PhotonColors.White)) {
        ClickableSubstringLink(
            text,
            PhotonColors.DarkGrey90,
            text.indexOf("link"),
            text.length
        ) { }
    }
}
