/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.storage

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.runBlocking
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class ThumbnailStorageTest {

    @Before
    @After
    fun cleanUp() {
        sharedDiskCache.clear(testContext)
    }

    @Test
    fun `clearThumbnails`() = runBlocking {
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext))

        thumbnailStorage.saveThumbnail("test-tab1", bitmap).joinBlocking()
        thumbnailStorage.saveThumbnail("test-tab2", bitmap).joinBlocking()
        var thumbnail1 = thumbnailStorage.loadThumbnail("test-tab1").await()
        var thumbnail2 = thumbnailStorage.loadThumbnail("test-tab2").await()
        assertNotNull(thumbnail1)
        assertNotNull(thumbnail2)

        thumbnailStorage.clearThumbnails()
        thumbnail1 = thumbnailStorage.loadThumbnail("test-tab1").await()
        thumbnail2 = thumbnailStorage.loadThumbnail("test-tab2").await()
        assertNull(thumbnail1)
        assertNull(thumbnail2)
    }

    @Test
    fun `deleteThumbnail`() = runBlocking {
        val sessionIdOrUrl = "test-tab1"
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext))

        thumbnailStorage.saveThumbnail(sessionIdOrUrl, bitmap).joinBlocking()
        var thumbnail = thumbnailStorage.loadThumbnail(sessionIdOrUrl).await()
        assertNotNull(thumbnail)

        thumbnailStorage.deleteThumbnail(sessionIdOrUrl).joinBlocking()
        thumbnail = thumbnailStorage.loadThumbnail(sessionIdOrUrl).await()
        assertNull(thumbnail)
    }

    @Test
    fun `saveThumbnail`() = runBlocking {
        val sessionIdOrUrl = "test-tab1"
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext))
        var thumbnail = thumbnailStorage.loadThumbnail(sessionIdOrUrl).await()

        assertNull(thumbnail)

        thumbnailStorage.saveThumbnail(sessionIdOrUrl, bitmap).joinBlocking()
        thumbnail = thumbnailStorage.loadThumbnail(sessionIdOrUrl).await()
        assertNotNull(thumbnail)
    }

    @Test
    fun `loadThumbnail`() = runBlocking {
        val sessionIdOrUrl = "test-tab1"
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext))

        thumbnailStorage.saveThumbnail(sessionIdOrUrl, bitmap)
        `when`(thumbnailStorage.loadThumbnail(sessionIdOrUrl)).thenReturn(CompletableDeferred(bitmap))

        val thumbnail = thumbnailStorage.loadThumbnail(sessionIdOrUrl).await()
        assertEquals(bitmap, thumbnail)
    }
}
