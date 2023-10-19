/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.text.ClickableText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.onClick
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.tooling.preview.Preview
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
 * @param linkTextStates The clickable part of the text.
 * @param style [TextStyle] applied to the text.
 * @param linkTextColor [Color] applied to the clickable part of the text.
 * @param linkTextDecoration [TextDecoration] applied to the clickable part of the text.
 */
@Composable
fun LinkText(
    text: String,
    linkTextStates: List<LinkTextState>,
    style: TextStyle = FirefoxTheme.typography.body2.copy(
        textAlign = TextAlign.Center,
        color = FirefoxTheme.colors.textSecondary,
    ),
    linkTextColor: Color = FirefoxTheme.colors.textAccent,
    linkTextDecoration: TextDecoration = TextDecoration.None,
) {
    val annotatedString = buildUrlAnnotatedString(
        text,
        linkTextStates,
        linkTextColor,
        linkTextDecoration,
    )

    // When using UrlAnnotation, talkback shows links in a separate dialog and
    // opens them in the default browser. Since this component allows the caller to define the
    // onClick behaviour - e.g. to open the link in in-app custom tab, here StringAnnotation is used
    // and modifier is enabled with Role.Button when screen reader is enabled.
    ClickableText(
        text = annotatedString,
        style = style,
        modifier = Modifier.semantics(mergeDescendants = true) {
            onClick {
                linkTextStates.firstOrNull()?.let {
                    it.onClick(it.url)
                }

                return@onClick true
            }
        },
        onClick = { charOffset ->
            onTextClick(annotatedString, charOffset, linkTextStates)
        },
    )
}

@VisibleForTesting
internal fun onTextClick(
    annotatedString: AnnotatedString,
    charOffset: Int,
    linkTextStates: List<LinkTextState>,
) {
    val range: AnnotatedString.Range<String>? =
        annotatedString.getStringAnnotations(URL_TAG, charOffset, charOffset).firstOrNull()
    range?.let { stringAnnotation ->
        val linkTextState = linkTextStates.firstOrNull {
            it.text == stringAnnotation.item
        }
        linkTextState?.let {
            it.onClick(it.url)
        }
    }
}

@VisibleForTesting
internal fun buildUrlAnnotatedString(
    text: String,
    linkTextStates: List<LinkTextState>,
    color: Color,
    decoration: TextDecoration,
) = buildAnnotatedString {
    append(text)

    linkTextStates.forEach {
        val startIndex = text.indexOf(it.text, ignoreCase = true)
        val endIndex = startIndex + it.text.length

        addStyle(
            style = SpanStyle(
                color = color,
                textDecoration = decoration,
            ),
            start = startIndex,
            end = endIndex,
        )

        addStringAnnotation(
            tag = URL_TAG,
            annotation = it.text,
            start = startIndex,
            end = endIndex,
        )
    }
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
            LinkText(text = "This is normal text, click here", linkTextStates = listOf(state))
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
            LinkText(text = "This is clickable text, followed by normal text", linkTextStates = listOf(state))
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
                linkTextStates = listOf(state),
                style = FirefoxTheme.typography.headline5,
            )
        }
    }
}

@Preview
@Composable
private fun LinkTextClickStyledPreview() {
    val state = LinkTextState(
        text = "clickable text",
        url = "www.mozilla.com",
        onClick = {},
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            LinkText(
                text = "This is clickable text, with underlined text",
                linkTextStates = listOf(state),
                style = FirefoxTheme.typography.headline5,
                linkTextColor = FirefoxTheme.colors.textOnColorSecondary,
                linkTextDecoration = TextDecoration.Underline,
            )
        }
    }
}

@Preview
@Composable
private fun MultipleLinksPreview() {
    val state1 = LinkTextState(
        text = "clickable text",
        url = "www.mozilla.com",
        onClick = {},
    )

    val state2 = LinkTextState(
        text = "another clickable text",
        url = "www.mozilla.com",
        onClick = {},
    )
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            LinkText(
                text = "This is clickable text, followed by normal text, followed by another clickable text",
                linkTextStates = listOf(state1, state2),
            )
        }
    }
}
