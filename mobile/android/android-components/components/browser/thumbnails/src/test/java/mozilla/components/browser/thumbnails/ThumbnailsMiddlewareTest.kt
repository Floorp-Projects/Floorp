/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ThumbnailsMiddlewareTest {

    @Test
    fun `thumbnail storage stores the provided thumbnail on update thumbnail action`() {
        val sessionIdOrUrl = "test-tab1"
        val tab = createTab("https://www.mozilla.org", id = "test-tab1")
        val thumbnailStorage: ThumbnailStorage = mock()
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(ThumbnailsMiddleware(thumbnailStorage))
        )

        val bitmap: Bitmap = mock()
        store.dispatch(ContentAction.UpdateThumbnailAction(sessionIdOrUrl, bitmap)).joinBlocking()
        verify(thumbnailStorage).saveThumbnail(sessionIdOrUrl, bitmap)
    }
}
