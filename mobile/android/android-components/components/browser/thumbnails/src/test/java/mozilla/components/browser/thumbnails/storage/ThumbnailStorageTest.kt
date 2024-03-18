/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.storage

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageSaveRequest
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
        privateDiskCache.clear(testContext)
    }

    @Test
    fun `clearThumbnails`() = runTestOnMain {
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext, testDispatcher))

        thumbnailStorage.saveThumbnail(ImageSaveRequest("test-tab1", false), bitmap).joinBlocking()
        thumbnailStorage.saveThumbnail(ImageSaveRequest("test-tab2", false), bitmap).joinBlocking()
        var thumbnail1 = thumbnailStorage.loadThumbnail(ImageLoadRequest("test-tab1", 100, false)).await()
        var thumbnail2 = thumbnailStorage.loadThumbnail(ImageLoadRequest("test-tab2", 100, false)).await()
        assertNotNull(thumbnail1)
        assertNotNull(thumbnail2)

        thumbnailStorage.clearThumbnails()
        thumbnail1 = thumbnailStorage.loadThumbnail(ImageLoadRequest("test-tab1", 100, false)).await()
        thumbnail2 = thumbnailStorage.loadThumbnail(ImageLoadRequest("test-tab2", 100, false)).await()
        assertNull(thumbnail1)
        assertNull(thumbnail2)
    }

    @Test
    fun `deleteThumbnail`() = runTestOnMain {
        val request = ImageSaveRequest("test-tab1", false)
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext, testDispatcher))

        thumbnailStorage.saveThumbnail(request, bitmap).joinBlocking()
        var thumbnail = thumbnailStorage.loadThumbnail(ImageLoadRequest(request.id, 100, request.isPrivate)).await()
        assertNotNull(thumbnail)

        thumbnailStorage.deleteThumbnail(request.id, request.isPrivate).joinBlocking()
        thumbnail = thumbnailStorage.loadThumbnail(ImageLoadRequest(request.id, 100, request.isPrivate)).await()
        assertNull(thumbnail)
    }

    @Test
    fun `saveThumbnail`() = runTestOnMain {
        val request = ImageLoadRequest("test-tab1", 100, false)
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext))
        var thumbnail = thumbnailStorage.loadThumbnail(request).await()

        assertNull(thumbnail)

        thumbnailStorage.saveThumbnail(ImageSaveRequest(request.id, request.isPrivate), bitmap).joinBlocking()
        thumbnail = thumbnailStorage.loadThumbnail(request).await()
        assertNotNull(thumbnail)
    }

    @Test
    fun `WHEN private save request THEN placed in private cache`() = runTestOnMain {
        val request = ImageLoadRequest("test-tab1", 100, true)
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext))
        var thumbnail = thumbnailStorage.loadThumbnail(request).await()

        assertNull(thumbnail)

        thumbnailStorage.saveThumbnail(ImageSaveRequest(request.id, request.isPrivate), bitmap).joinBlocking()
        thumbnail = thumbnailStorage.loadThumbnail(request).await()
        assertNotNull(thumbnail)
    }

    @Test
    fun `loadThumbnail`() = runTestOnMain {
        val request = ImageLoadRequest("test-tab1", 100, false)
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext, testDispatcher))

        thumbnailStorage.saveThumbnail(ImageSaveRequest(request.id, request.isPrivate), bitmap)
        `when`(thumbnailStorage.loadThumbnail(request)).thenReturn(CompletableDeferred(bitmap))

        val thumbnail = thumbnailStorage.loadThumbnail(request).await()
        assertEquals(bitmap, thumbnail)
    }

    @Test
    fun `WHEN private load request THEN loaded from private cache`() = runTestOnMain {
        val request = ImageLoadRequest("test-tab1", 100, true)
        val bitmap: Bitmap = mock()
        val thumbnailStorage = spy(ThumbnailStorage(testContext, testDispatcher))

        thumbnailStorage.saveThumbnail(ImageSaveRequest(request.id, request.isPrivate), bitmap)
        `when`(thumbnailStorage.loadThumbnail(request)).thenReturn(CompletableDeferred(bitmap))

        val thumbnail = thumbnailStorage.loadThumbnail(request).await()
        assertEquals(bitmap, thumbnail)
        assertNull(thumbnailStorage.loadThumbnail(ImageLoadRequest(request.id, request.size, false)).await())
    }

    @Test
    fun `WHEN storage is initialized THEN private cache is cleared`() {
        val request = ImageLoadRequest("test-tab1", 100, true)
        val bitmap: Bitmap = mock()

        privateDiskCache.putThumbnailBitmap(testContext, ImageSaveRequest(request.id, request.isPrivate), bitmap)
        assertNotNull(privateDiskCache.getThumbnailData(testContext, request))
        ThumbnailStorage(testContext, testDispatcher)

        assertNull(privateDiskCache.getThumbnailData(testContext, request))
    }
}
