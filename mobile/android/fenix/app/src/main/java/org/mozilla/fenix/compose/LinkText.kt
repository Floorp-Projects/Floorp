/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.text.ClickableText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import mozilla.components.support.ktx.android.content.isScreenReaderEnabled
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The tag used for links in the text for annotated strings.
 */
private const val URL_TAG = "URL_TAG"

/**
 * Model containing link text, url and action.
 *
 * @property text Substring of the text passed to [LinkText] to be displayed as clickable link.
 * @property url Url the link should point to.
 * @property onClick Callback to be invoked when link is clicked.
 */
data class LinkTextState(
    val text: String,
    val url: String,
    val onClick: (String) -> Unit,
)

/**
 * A composable for displaying text that contains a clickable link text.
 *
 * @param text The complete text.
 * @param linkTextState The clickable part of the text.
 * @param style [TextStyle] applied to the text.
 */
@Composable
fun LinkText(
    text: String,
    linkTextState: LinkTextState,
    style: TextStyle = FirefoxTheme.typography.body2.copy(
        textAlign = TextAlign.Center,
        color = FirefoxTheme.colors.textSecondary,
    ),
) {
    val context = LocalContext.current
    val annotatedString = buildAnnotatedString {
        val startIndex = text.indexOf(linkTextState.text, ignoreCase = true)
        val endIndex = startIndex + linkTextState.text.length

        append(text)

        addStyle(
            style = SpanStyle(color = FirefoxTheme.colors.textAccent),
            start = startIndex,
            end = endIndex,
        )

        addStringAnnotation(
            tag = URL_TAG,
            annotation = linkTextState.url,
            start = startIndex,
            end = endIndex,
        )
    }

    // When using UrlAnnotation, talkback shows links in a separate dialog and
    // opens them in the default browser. Since this component allows the caller to define the
    // onClick behaviour - e.g. to open the link in in-app custom tab, here StringAnnotation is used
    // and modifier is enabled with Role.Button when screen reader is enabled.
    ClickableText(
        text = annotatedString,
        style = style,
        modifier = Modifier.clickable(
            enabled = context.isScreenReaderEnabled,
            role = Role.Button,
            onClickLabel = linkTextState.text,
            onClick = { linkTextState.onClick(linkTextState.url) },
        ),
        onClick = {
            if (!context.isScreenReaderEnabled) {
                val range: AnnotatedString.Range<String>? =
                    annotatedString.getStringAnnotations(URL_TAG, it, it).firstOrNull()
                range?.let { stringAnnotation ->
                    linkTextState.onClick(stringAnnotation.item)
                }
            }
        },
    )
}

@Preview
@Composable
private fun LinkTextEndPreview() {
    val state = LinkTextState(
        text = "click here",
        url = "www.mozilla.com",
        onClick = {},
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            LinkText(text = "This is normal text, click here", linkTextState = state)
        }
    }
}

@Preview
@Composable
private fun LinkTextMiddlePreview() {
    val state = LinkTextState(
        text = "clickable text",
        url = "www.mozilla.com",
        onClick = {},
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            LinkText(text = "This is clickable text, followed by normal text", linkTextState = state)
        }
    }
}

@Preview
@Composable
private fun LinkTextStyledPreview() {
    val state = LinkTextState(
        text = "clickable text",
        url = "www.mozilla.com",
        onClick = {},
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            LinkText(
                text = "This is clickable text, in a different style",
                linkTextState = state,
                style = FirefoxTheme.typography.headline5,
            )
        }
    }
}
