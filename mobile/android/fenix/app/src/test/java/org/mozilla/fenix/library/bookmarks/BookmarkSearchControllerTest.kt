/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity

class BookmarkSearchControllerTest {

    @MockK(relaxed = true) private lateinit var activity: HomeActivity
    @MockK(relaxed = true) private lateinit var store: BookmarkSearchFragmentStore

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
    }

    @Test
    fun `WHEN editing is cancelled THEN clearToolbarFocus is called`() = runTest {
        var clearToolbarFocusInvoked = false
        createController(
            clearToolbarFocus = {
                clearToolbarFocusInvoked = true
            }
        ).handleEditingCancelled()

        assertTrue(clearToolbarFocusInvoked)
    }

    @Test
    fun `WHEN text changed THEN update query action is dispatched`() {
        val text = "fenix"

        createController().handleTextChanged(text)

        verify { store.dispatch(BookmarkSearchFragmentAction.UpdateQuery(text)) }
    }

    @Test
    fun `WHEN text is changed to empty THEN update query action is dispatched`() {
        val text = ""

        createController().handleTextChanged(text)

        verify { store.dispatch(BookmarkSearchFragmentAction.UpdateQuery(text)) }
    }

    @Test
    fun `WHEN url is tapped THEN openToBrowserAndLoad is called`() {
        val url = "https://www.google.com/"
        val flags = EngineSession.LoadUrlFlags.none()

        createController().handleUrlTapped(url, flags)
        createController().handleUrlTapped(url)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = true,
                from = BrowserDirection.FromBookmarkSearchDialog,
                flags = flags
            )
        }
    }

    private fun createController(
        clearToolbarFocus: () -> Unit = { },
    ): BookmarkSearchDialogController {
        return BookmarkSearchDialogController(
            activity = activity,
            fragmentStore = store,
            clearToolbarFocus = clearToolbarFocus,
        )
    }
}
