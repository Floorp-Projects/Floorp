/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.internal

import android.view.View
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ActionContainerTest {
    private lateinit var actionContainer: ActionContainer
    private lateinit var browserAction: Toolbar.Action

    @Before
    fun setUp() {
        browserAction = BrowserToolbar.Button(
            imageDrawable = mock(),
            contentDescription = "Test",
            visible = { true },
            autoHide = { true },
            weight = { 2 },
            listener = mock(),
        )
        actionContainer = ActionContainer(testContext)
    }

    @Test
    fun `GIVEN multiple actions with different weights WHEN calculateInsertionIndex is called THEN action is placed at right index`() {
        actionContainer.addAction(
            BrowserToolbar.Button(
                imageDrawable = mock(),
                contentDescription = "Share",
                visible = { true },
                weight = { 1 },
                listener = mock(),
            ),
        )
        actionContainer.addAction(
            BrowserToolbar.Button(
                imageDrawable = mock(),
                contentDescription = "Reload",
                visible = { true },
                weight = { 3 },
                listener = mock(),
            ),
        )
        val newAction =
            BrowserToolbar.Button(
                imageDrawable = mock(),
                contentDescription = "Translation",
                visible = { true },
                weight = { 2 },
                listener = mock(),
            )

        val insertionIndex = actionContainer.calculateInsertionIndex(newAction)

        assertEquals("The insertion index should be", 1, insertionIndex)
    }

    @Test
    fun `WHEN addAction is called THEN child views are increased`() {
        actionContainer.addAction(browserAction)

        assertEquals(1, actionContainer.childCount)
    }

    @Test
    fun `WHEN removeAction is called THEN child views are decreased`() {
        actionContainer.addAction(browserAction)
        actionContainer.removeAction(browserAction)

        assertEquals(0, actionContainer.childCount)
    }

    @Test
    fun `WHEN invalidateAction is called THEN action visibility is reconsidered`() {
        val browserToolbarAction = BrowserToolbar.Button(
            imageDrawable = mock(),
            contentDescription = "Translation",
            visible = { false },
            weight = { 2 },
            listener = mock(),
        )
        actionContainer.addAction(browserToolbarAction)
        actionContainer.invalidateActions()

        assertEquals(View.GONE, actionContainer.visibility)
    }
}
