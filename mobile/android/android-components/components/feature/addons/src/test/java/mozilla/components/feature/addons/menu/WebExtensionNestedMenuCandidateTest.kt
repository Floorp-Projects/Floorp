/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.menu

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.AsyncDrawableMenuIcon
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.NestedMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuIcon
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class WebExtensionNestedMenuCandidateTest {

    private val pageAction = Action(
        title = "page title",
        loadIcon = { mock() },
        enabled = true,
        badgeText = "pageBadge",
        badgeTextColor = Color.WHITE,
        badgeBackgroundColor = Color.BLUE,
    ) {}

    private val browserAction = Action(
        title = "browser title",
        loadIcon = { mock() },
        enabled = true,
        badgeText = "browserBadge",
        badgeTextColor = Color.WHITE,
        badgeBackgroundColor = Color.BLUE,
    ) {}

    @Test
    fun `create nested menu from browser extensions and actions`() {
        val state = BrowserState(
            extensions = mapOf(
                "1" to WebExtensionState(id = "1", browserAction = browserAction, pageAction = pageAction),
            ),
        )
        val candidate = state.createWebExtensionMenuCandidate(
            testContext,
            appendExtensionSubMenuAt = Side.END,
        ) as NestedMenuCandidate

        assertEquals(6, candidate.subMenuItems!!.size)

        assertEquals(
            "Add-ons Manager",
            (candidate.subMenuItems!![0] as TextMenuCandidate).text,
        )
        assertEquals(
            DividerMenuCandidate(),
            candidate.subMenuItems!![1],
        )

        val ext1 = candidate.subMenuItems!![2] as TextMenuCandidate
        assertTrue(ext1.containerStyle.isEnabled)
        assertEquals("browser title", ext1.text)
        assertTrue(ext1.start is AsyncDrawableMenuIcon)
        assertEquals(
            "browserBadge",
            (ext1.end as TextMenuIcon).text,
        )

        val ext2 = candidate.subMenuItems!![3] as TextMenuCandidate
        assertTrue(ext2.containerStyle.isEnabled)
        assertEquals("page title", ext2.text)
        assertTrue(ext2.start is AsyncDrawableMenuIcon)
        assertEquals(
            "pageBadge",
            (ext2.end as TextMenuIcon).text,
        )

        assertEquals(
            DividerMenuCandidate(),
            candidate.subMenuItems!![4],
        )
        assertEquals(
            "Add-ons",
            (candidate.subMenuItems!![5] as NestedMenuCandidate).text,
        )
    }

    @Test
    fun `browser actions can be overridden per tab`() {
        val pageActionOverride = Action(
            title = "updatedTitle",
            loadIcon = null,
            enabled = true,
            badgeText = "updatedText",
            badgeTextColor = Color.RED,
            badgeBackgroundColor = Color.GREEN,
        ) {}
        val browserActionOverride = Action(
            title = "updatedTitle",
            loadIcon = null,
            enabled = false,
            badgeText = "updatedText",
            badgeTextColor = Color.RED,
            badgeBackgroundColor = Color.GREEN,
        ) {}

        val state = BrowserState(
            extensions = mapOf(
                "1" to WebExtensionState(id = "1", browserAction = browserAction, pageAction = pageAction),
            ),
            tabs = listOf(
                createTab(
                    id = "tab-1",
                    url = "https://mozilla.org",
                    extensions = mapOf(
                        "1" to WebExtensionState(
                            id = "1",
                            browserAction = browserActionOverride,
                            pageAction = pageActionOverride,
                        ),
                    ),
                ),
            ),
        )
        val candidate = state.createWebExtensionMenuCandidate(
            testContext,
            tabId = "tab-1",
            appendExtensionSubMenuAt = Side.START,
        ) as NestedMenuCandidate

        assertEquals(6, candidate.subMenuItems!!.size)

        assertEquals(
            "Add-ons",
            (candidate.subMenuItems!![0] as NestedMenuCandidate).text,
        )
        assertNull((candidate.subMenuItems!![0] as NestedMenuCandidate).subMenuItems)
        assertEquals(
            DividerMenuCandidate(),
            candidate.subMenuItems!![1],
        )

        val ext1 = candidate.subMenuItems!![2] as TextMenuCandidate
        assertFalse(ext1.containerStyle.isEnabled)
        assertEquals("updatedTitle", ext1.text)
        assertEquals(
            "updatedText",
            (ext1.end as TextMenuIcon).text,
        )

        val ext2 = candidate.subMenuItems!![3] as TextMenuCandidate
        assertTrue(ext2.containerStyle.isEnabled)
        assertEquals("updatedTitle", ext2.text)
        assertEquals(
            "updatedText",
            (ext2.end as TextMenuIcon).text,
        )

        assertEquals(
            DividerMenuCandidate(),
            candidate.subMenuItems!![4],
        )
        assertEquals(
            "Add-ons Manager",
            (candidate.subMenuItems!![5] as TextMenuCandidate).text,
        )
    }
}
