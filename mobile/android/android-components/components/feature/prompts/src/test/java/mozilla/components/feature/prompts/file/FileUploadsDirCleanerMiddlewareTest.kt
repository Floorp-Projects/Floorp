/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class FileUploadsDirCleanerMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    @Test
    fun `WHEN an action that indicates the user has navigated to another webiste THEN clean up temporary uploads`() {
        val fileUploadsDirCleaner = mock<FileUploadsDirCleaner>()
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = listOf(
                FileUploadsDirCleanerMiddleware(
                    fileUploadsDirCleaner = fileUploadsDirCleaner,
                ),
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(TabListAction.SelectTabAction("test-tab")).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(fileUploadsDirCleaner).cleanRecentUploads()

        store.dispatch(ContentAction.UpdateLoadRequestAction("test-tab", mock())).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(fileUploadsDirCleaner, times(2)).cleanRecentUploads()

        store.dispatch(ContentAction.UpdateUrlAction("test-tab", "url")).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(fileUploadsDirCleaner, times(3)).cleanRecentUploads()
    }
}
