/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi // for runTestOnMain
@RunWith(AndroidJUnit4::class)
class ClipboardSuggestionProviderTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val clipboardManager: ClipboardManager
        get() = testContext.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    @Test
    fun `provider returns empty list by default`() = runTestOnMain {
        clipboardManager.clearPrimaryClip()

        val provider = ClipboardSuggestionProvider(testContext, mock())

        provider.onInputStarted()
        val suggestions = provider.onInputChanged("Hello")

        assertEquals(0, suggestions.size)
    }

    @Test
    fun `provider returns empty list for non plain text clip`() = runTestOnMain {
        clipboardManager.setPrimaryClip(
            ClipData.newHtmlText(
                "Label",
                "Hello mozilla.org",
                "<b>This is HTML on mozilla.org</b>",
            ),
        )

        assertNull(getSuggestion())
    }

    @Test
    fun `provider should return suggestion if clipboard contains url`() = runTestOnMain {
        assertClipboardYieldsUrl(
            "https://www.mozilla.org",
            "https://www.mozilla.org",
        )

        assertClipboardYieldsUrl(
            "https : //mozilla.org is a broken firefox.com URL",
            "mozilla.org",
        )

        assertClipboardYieldsUrl(
            """
                This is a longer
                text over multiple lines
                and it https://www.mozilla.org contains
                URLs as well. https://www.firefox.com
            """,
            "https://www.mozilla.org",
        )

        assertClipboardYieldsUrl(
            """
                This is a longer
                text over multiple lines
                and it www.mozilla.org contains
                URLs as well. https://www.firefox.com
            """,
            "https://www.firefox.com",
        )

        assertClipboardYieldsUrl(
            """
            mozilla.org
            firefox.com
            mozilla.org/en-US/firefox/developer/
            """,
            "mozilla.org",
        )

        // Note that the new, less-lenient URL detection process (Issue #5594) allows the dot
        // at the end of the IP address to be part of the URL. Gecko handles this.
        assertClipboardYieldsUrl("My IP is 192.168.0.1.", "192.168.0.1.")
    }

    @Test
    fun `provider return suggestion on input start`() = runTestOnMain {
        clipboardManager.setPrimaryClip(ClipData.newPlainText("Test label", "https://www.mozilla.org"))

        val provider = ClipboardSuggestionProvider(testContext, mock())
        val suggestions = provider.onInputStarted()

        assertEquals(1, suggestions.size)

        val suggestion = suggestions.firstOrNull()
        assertNotNull(suggestion!!)

        assertEquals("https://www.mozilla.org", suggestion.description)
    }

    @Test
    fun `provider should return no suggestions if clipboard does not contain a url`() = runTestOnMain {
        assertClipboardYieldsNothing("Hello World")

        assertClipboardYieldsNothing("Is this mozilla org")
    }

    @Test
    fun `provider should allow customization of title and icon on suggestion`() = runTestOnMain {
        clipboardManager.setPrimaryClip(ClipData.newPlainText("Test label", "http://mozilla.org"))
        val bitmap = Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888)
        val provider = ClipboardSuggestionProvider(
            testContext,
            mock(),
            title = "My test title",
            icon = bitmap,
            requireEmptyText = false,
        )

        val suggestion = run {
            provider.onInputStarted()
            val suggestions = provider.onInputChanged("Hello")

            suggestions.firstOrNull()
        }

        assertEquals(bitmap, suggestion?.icon)
        assertEquals("My test title", suggestion?.title)
    }

    @Test
    fun `clicking suggestion loads url`() = runTestOnMain {
        clipboardManager.setPrimaryClip(
            ClipData.newPlainText(
                "Label",
                "Hello Mozilla, https://www.mozilla.org",
            ),
        )

        val useCase: SessionUseCases.LoadUrlUseCase = mock()

        val provider = ClipboardSuggestionProvider(testContext, useCase, requireEmptyText = false)

        provider.onInputStarted()
        val suggestions = provider.onInputChanged("Hello")
        assertEquals(1, suggestions.size)

        val suggestion = suggestions.first()

        verify(useCase, never()).invoke(any(), any(), any())

        assertNotNull(suggestion.onSuggestionClicked)
        suggestion.onSuggestionClicked!!.invoke()

        verify(useCase).invoke(eq("https://www.mozilla.org"), any(), any())
    }

    @Test
    fun `provider returns empty list for non-empty text if empty text required`() = runTestOnMain {
        clipboardManager.setPrimaryClip(
            ClipData.newPlainText(
                "Label",
                "Hello Mozilla, https://www.mozilla.org",
            ),
        )

        val provider = ClipboardSuggestionProvider(testContext, mock(), requireEmptyText = true)
        val suggestions = provider.onInputChanged("Hello")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `provider calls speculative connect for URL of suggestion`() = runTestOnMain {
        val engine: Engine = mock()
        val provider = ClipboardSuggestionProvider(testContext, mock(), engine = engine)
        var suggestions = provider.onInputStarted()
        assertTrue(suggestions.isEmpty())
        verify(engine, never()).speculativeConnect(anyString())

        clipboardManager.setPrimaryClip(ClipData.newPlainText("Test label", "https://www.mozilla.org"))
        suggestions = provider.onInputStarted()
        assertEquals(1, suggestions.size)
        verify(engine, times(1)).speculativeConnect(eq("https://www.mozilla.org"))

        val suggestion = suggestions.firstOrNull()
        assertNotNull(suggestion!!)
        assertEquals("https://www.mozilla.org", suggestion.description)
    }

    private suspend fun assertClipboardYieldsUrl(text: String, url: String) {
        val suggestion = getSuggestionWithClipboard(text)

        assertNotNull(suggestion)

        assertEquals(url, suggestion!!.description)
    }

    private suspend fun assertClipboardYieldsNothing(text: String) {
        val suggestion = getSuggestionWithClipboard(text)
        assertNull(suggestion)
    }

    private suspend fun getSuggestionWithClipboard(text: String): AwesomeBar.Suggestion? {
        clipboardManager.setPrimaryClip(ClipData.newPlainText("Test label", text))
        return getSuggestion()
    }

    private suspend fun getSuggestion(): AwesomeBar.Suggestion? {
        val provider = ClipboardSuggestionProvider(testContext, mock(), requireEmptyText = false)

        provider.onInputStarted()
        val suggestions = provider.onInputChanged("Hello")

        return suggestions.firstOrNull()
    }
}
