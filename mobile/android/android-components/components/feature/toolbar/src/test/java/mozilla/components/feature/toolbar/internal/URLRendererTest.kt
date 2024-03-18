/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar.internal

import android.graphics.Color
import android.text.SpannableStringBuilder
import android.text.style.ForegroundColorSpan
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class URLRendererTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `Lifecycle methods start and stop job`() {
        val renderer = URLRenderer(mock(), mock())

        assertNull(renderer.job)

        renderer.start()

        assertNotNull(renderer.job)
        assertTrue(renderer.job!!.isActive)

        renderer.stop()

        assertNotNull(renderer.job)
        assertFalse(renderer.job!!.isActive)
    }

    @Test
    fun `Render with configuration`() {
        runTestOnMain {
            val configuration = ToolbarFeature.UrlRenderConfiguration(
                publicSuffixList = PublicSuffixList(testContext, Dispatchers.Unconfined),
                registrableDomainColor = Color.RED,
                urlColor = Color.GREEN,
            )

            val toolbar: Toolbar = mock()

            val renderer = URLRenderer(toolbar, configuration)

            renderer.updateUrl("https://www.mozilla.org/")

            val captor = argumentCaptor<CharSequence>()
            verify(toolbar).url = captor.capture()

            assertNotNull(captor.value)
            assertTrue(captor.value is SpannableStringBuilder)
            val url = captor.value as SpannableStringBuilder

            assertEquals("https://www.mozilla.org/", url.toString())

            val spans = url.getSpans(0, url.length, ForegroundColorSpan::class.java)

            assertEquals(2, spans.size)
            assertEquals(Color.GREEN, spans[0].foregroundColor)
            assertEquals(Color.RED, spans[1].foregroundColor)

            val domain = url.subSequence(12, 23)
            assertEquals("mozilla.org", domain.toString())

            val domainSpans = url.getSpans(13, 23, ForegroundColorSpan::class.java)
            assertEquals(2, domainSpans.size)
            assertEquals(Color.GREEN, domainSpans[0].foregroundColor)
            assertEquals(Color.RED, domainSpans[1].foregroundColor)

            val prefix = url.subSequence(0, 12)
            assertEquals("https://www.", prefix.toString())

            val prefixSpans = url.getSpans(0, 12, ForegroundColorSpan::class.java)
            assertEquals(1, prefixSpans.size)
            assertEquals(Color.GREEN, prefixSpans[0].foregroundColor)

            val suffix = url.subSequence(23, url.length)
            assertEquals("/", suffix.toString())

            val suffixSpans = url.getSpans(23, url.length, ForegroundColorSpan::class.java)
            assertEquals(1, suffixSpans.size)
            assertEquals(Color.GREEN, suffixSpans[0].foregroundColor)
        }
    }
}
