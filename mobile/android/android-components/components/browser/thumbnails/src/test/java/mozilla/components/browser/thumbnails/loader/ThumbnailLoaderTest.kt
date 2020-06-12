/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.loader

import android.graphics.Bitmap
import android.graphics.drawable.Drawable
import android.widget.ImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.thumbnails.R
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ThumbnailLoaderTest {
    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        Dispatchers.setMain(testDispatcher)
    }

    @After
    fun teardown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `automatically load thumbnails into image view`() {
        val mockedBitmap: Bitmap = mock()
        val result = CompletableDeferred<Bitmap>()
        val view: ImageView = mock()
        val storage: ThumbnailStorage = mock()
        val loader = spy(ThumbnailLoader(storage))

        doReturn(result).`when`(storage).loadThumbnail("123")

        loader.loadIntoView(view, "123")

        verify(view).setImageDrawable(null)
        verify(view).addOnAttachStateChangeListener(any())
        verify(view).setTag(eq(R.id.mozac_browser_thumbnails_tag_job), any())
        verify(view, never()).setImageBitmap(any())

        result.complete(mockedBitmap)

        verify(view).setImageBitmap(mockedBitmap)
        verify(view).removeOnAttachStateChangeListener(any())
        verify(view).setTag(R.id.mozac_browser_thumbnails_tag_job, null)
    }

    @Test
    fun `loadIntoView sets drawable to error if cancelled`() {
        val result = CompletableDeferred<Bitmap>()
        val view: ImageView = mock()
        val placeholder: Drawable = mock()
        val error: Drawable = mock()
        val storage: ThumbnailStorage = mock()
        val loader = spy(ThumbnailLoader(storage))

        doReturn(result).`when`(storage).loadThumbnail("123")

        loader.loadIntoView(view, "123", placeholder = placeholder, error = error)

        verify(view).setImageDrawable(placeholder)

        result.cancel()

        verify(view).setImageDrawable(error)
        verify(view).removeOnAttachStateChangeListener(any())
        verify(view).setTag(R.id.mozac_browser_thumbnails_tag_job, null)
    }

    @Test
    fun `loadIntoView cancels previous jobs`() {
        val result = CompletableDeferred<Bitmap>()
        val view: ImageView = mock()
        val previousJob: Job = mock()
        val storage: ThumbnailStorage = mock()
        val loader = spy(ThumbnailLoader(storage))

        doReturn(previousJob).`when`(view).getTag(R.id.mozac_browser_thumbnails_tag_job)
        doReturn(result).`when`(storage).loadThumbnail("123")

        loader.loadIntoView(view, "123")

        verify(previousJob).cancel()

        result.cancel()
    }
}
