/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ThumbnailsUseCasesTest {

    @Test
    fun `LoadThumbnailUseCase - loads the thumbnail from the in-memory ContentState if available`() = runBlocking {
        val sessionIdOrUrl = "test-tab1"
        val bitmap: Bitmap = mock()
        val tab = createTab("https://www.mozilla.org", id = "test-tab1", thumbnail = bitmap)
        val thumbnailStorage: ThumbnailStorage = mock()
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(ThumbnailsMiddleware(thumbnailStorage))
        )
        val useCases = ThumbnailsUseCases(store, thumbnailStorage)

        val thumbnail = useCases.loadThumbnail(sessionIdOrUrl)
        assertEquals(bitmap, thumbnail)
    }

    @Test
    fun `LoadThumbnailUseCase - loads the thumbnail from the disk cache if in-memory thumbnail is unavailable`() = runBlocking {
        val sessionIdOrUrl = "test-tab1"
        val bitmap: Bitmap = mock()
        val tab = createTab("https://www.mozilla.org", id = "test-tab1")
        val thumbnailStorage: ThumbnailStorage = mock()
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(ThumbnailsMiddleware(thumbnailStorage))
        )
        val useCases = ThumbnailsUseCases(store, thumbnailStorage)

        `when`(thumbnailStorage.loadThumbnail(any())).thenReturn(CompletableDeferred(bitmap))

        val thumbnail = useCases.loadThumbnail(sessionIdOrUrl)
        verify(thumbnailStorage).loadThumbnail(sessionIdOrUrl)
        assertEquals(bitmap, thumbnail)
    }
}
