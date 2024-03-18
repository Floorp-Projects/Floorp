/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.menu

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.menu.candidate.AsyncDrawableMenuIcon
import mozilla.components.concept.menu.candidate.TextMenuIcon
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class WebExtensionActionMenuCandidateTest {

    private val baseAction = Action(
        title = "action",
        enabled = false,
        loadIcon = null,
        badgeText = "",
        badgeTextColor = 0,
        badgeBackgroundColor = 0,
        onClick = {},
    )

    @Test
    fun `create menu candidate from null action`() {
        val onClick = {}
        val candidate = Action(
            title = null,
            enabled = null,
            loadIcon = null,
            badgeText = null,
            badgeTextColor = null,
            badgeBackgroundColor = null,
            onClick = onClick,
        ).createMenuCandidate(testContext)

        assertEquals("", candidate.text)
        assertFalse(candidate.containerStyle.isEnabled)
        assertEquals(onClick, candidate.onClick)

        assertNull(candidate.start)
        assertNull(candidate.end)
    }

    @Test
    fun `create menu candidate with text and no badge`() {
        val candidate = baseAction
            .copy(badgeText = null)
            .createMenuCandidate(testContext)

        assertEquals("action", candidate.text)
        assertFalse(candidate.containerStyle.isEnabled)

        assertNull(candidate.start)
        assertNull(candidate.end)
    }

    @Test
    fun `create menu candidate with badge`() {
        val candidate = baseAction
            .copy(
                badgeText = "10",
                badgeTextColor = Color.DKGRAY,
                badgeBackgroundColor = Color.YELLOW,
                enabled = true,
            )
            .createMenuCandidate(testContext)

        assertEquals("action", candidate.text)
        assertTrue(candidate.containerStyle.isEnabled)

        assertNull(candidate.start)
        assertTrue(candidate.end is TextMenuIcon)

        assertEquals(
            TextMenuIcon(
                text = "10",
                backgroundTint = Color.YELLOW,
                textStyle = TextStyle(color = Color.DKGRAY),
            ),
            candidate.end,
        )
    }

    @Test
    fun `create menu candidate with icon`() = runTest {
        var calledWith: Int = -1
        val candidate = baseAction
            .copy(
                badgeText = null,
                loadIcon = { height ->
                    calledWith = height
                    null
                },
            )
            .createMenuCandidate(testContext)

        assertEquals("action", candidate.text)
        assertFalse(candidate.containerStyle.isEnabled)

        assertTrue(candidate.start is AsyncDrawableMenuIcon)
        assertNull(candidate.end)

        val start = candidate.start as AsyncDrawableMenuIcon
        assertNotNull(start.loadingDrawable)
        assertNotNull(start.fallbackDrawable)
        assertNull(start.effect)

        assertNull(start.loadDrawable(40, 30))
        assertEquals(30, calledWith)
    }
}
