/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextDecoration
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LinkTextTest {

    @Test
    fun `WHEN attempting a click on non-annotated text THEN no links are clicked`() {
        var linkClicked = false
        val linkTextState = LinkTextState(
            text = "clickable text",
            url = "www.mozilla.com",
            onClick = { linkClicked = true },
        )
        val annotatedString = buildUrlAnnotatedString(
            text = "This is not clickable text, but this is ${linkTextState.text}",
            linkTextStates = listOf(linkTextState),
            color = Color(0xFF000000),
            decoration = TextDecoration.None,
        )

        onTextClick(
            annotatedString,
            2,
            listOf(linkTextState),
        )

        assertFalse(linkClicked)
    }

    @Test
    fun `WHEN attempting a click on clickable annotated text THEN the corresponding link is clicked`() {
        val linkClicked = mutableListOf(false, false, false)
        val linkTextStates = listOf(
            LinkTextState(
                text = "click1",
                url = "www.mozilla.com",
                onClick = { linkClicked[0] = true },
            ),
            LinkTextState(
                text = "click2",
                url = "www.mozilla.com",
                onClick = { linkClicked[1] = true },
            ),
            LinkTextState(
                text = "click3",
                url = "www.mozilla.com",
                onClick = { linkClicked[2] = true },
            ),
        )
        val annotatedString = buildUrlAnnotatedString(
            text = "This is not clickable text, but these are clickable texts: " + linkTextStates.map { it.text + ", " },
            linkTextStates = linkTextStates,
            color = Color(0xFF000000),
            decoration = TextDecoration.None,
        )

        onTextClick(
            annotatedString,
            70,
            linkTextStates,
        )

        assertFalse(linkClicked[0])
        assertFalse(linkClicked[2])
        assertTrue(linkClicked[1])
    }
}
