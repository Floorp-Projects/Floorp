/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.layout

import android.graphics.Bitmap
import android.widget.ImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.test.mock
import mozilla.utils.setupTestCoroutinesDispatcher
import mozilla.utils.unsetTestCoroutinesDispatcher
import org.junit.After
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class IconLoaderTest {

    @Before
    fun setUp() {
        setupTestCoroutinesDispatcher()
    }

    @After
    fun tearDown() {
        unsetTestCoroutinesDispatcher()
    }

    @Test
    fun `loads image and sets it on ImageView`() {
        runBlocking {
            val bitmap: Bitmap = mock()
            val suggestion = AwesomeBar.Suggestion(
                provider = mock(),
                icon = { _, _ -> bitmap })

            val view: ImageView = mock()
            val loader = IconLoader(view)

            loader.load(suggestion)

            assertNotNull(loader.iconJob)
            loader.iconJob!!.join()

            verify(view).setImageBitmap(bitmap)
        }
    }

    @Test
    fun `Load task is cancelled`() {
        runBlocking {
            @Suppress("UNREACHABLE_CODE")
            val suggestion = AwesomeBar.Suggestion(
                provider = mock(),
                icon = { _, _ ->
                    while (true) { delay(10) }
                    null
                })

            val view: ImageView = mock()
            val loader = IconLoader(view)

            delay(100)

            loader.load(suggestion)

            loader.cancel()

            withTimeout(5000) {
                loader.iconJob!!.join()
            }

            assertTrue(loader.iconJob!!.isCancelled)
        }
    }
}
