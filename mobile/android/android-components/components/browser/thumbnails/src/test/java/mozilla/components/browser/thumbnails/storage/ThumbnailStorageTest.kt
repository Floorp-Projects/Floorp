/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.storage

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class ThumbnailStorageTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testDispatcher = coroutinesTestRule.testDispatcher

    @Before
    @After
    fun cleanUp() {
        sharedDiskCache.clear(testContext)
    }

    @Test
    fun `clearThumbnails`() = runTestOnMain {
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext, testDispatcher))

        thumbnailStorage.saveThumbnail("test-tab1", bitmap).joinBlocking()
        thumbnailStorage.saveThumbnail("test-tab2", bitmap).joinBlocking()
        var thumbnail1 = thumbnailStorage.loadThumbnail(ImageLoadRequest("test-tab1", 100)).await()
        var thumbnail2 = thumbnailStorage.loadThumbnail(ImageLoadRequest("test-tab2", 100)).await()
        assertNotNull(thumbnail1)
        assertNotNull(thumbnail2)

        thumbnailStorage.clearThumbnails()
        thumbnail1 = thumbnailStorage.loadThumbnail(ImageLoadRequest("test-tab1", 100)).await()
        thumbnail2 = thumbnailStorage.loadThumbnail(ImageLoadRequest("test-tab2", 100)).await()
        assertNull(thumbnail1)
        assertNull(thumbnail2)
    }

    @Test
    fun `deleteThumbnail`() = runTestOnMain {
        val request = "test-tab1"
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext, testDispatcher))

        thumbnailStorage.saveThumbnail(request, bitmap).joinBlocking()
        var thumbnail = thumbnailStorage.loadThumbnail(ImageLoadRequest(request, 100)).await()
        assertNotNull(thumbnail)

        thumbnailStorage.deleteThumbnail(request).joinBlocking()
        thumbnail = thumbnailStorage.loadThumbnail(ImageLoadRequest(request, 100)).await()
        assertNull(thumbnail)
    }

    @Test
    fun `saveThumbnail`() = runTestOnMain {
        val request = ImageLoadRequest("test-tab1", 100)
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext))
        var thumbnail = thumbnailStorage.loadThumbnail(request).await()

        assertNull(thumbnail)

        thumbnailStorage.saveThumbnail(request.id, bitmap).joinBlocking()
        thumbnail = thumbnailStorage.loadThumbnail(request).await()
        assertNotNull(thumbnail)
    }

    @Test
    fun `loadThumbnail`() = runTestOnMain {
        val request = ImageLoadRequest("test-tab1", 100)
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext, testDispatcher))

        thumbnailStorage.saveThumbnail(request.id, bitmap)
        `when`(thumbnailStorage.loadThumbnail(request)).thenReturn(CompletableDeferred(bitmap))

        val thumbnail = thumbnailStorage.loadThumbnail(request).await()
        assertEquals(bitmap, thumbnail)
    }
}
