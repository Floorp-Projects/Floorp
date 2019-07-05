/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ClipboardSuggestionProviderTest {

    private val clipboardManager: ClipboardManager
        get() = testContext.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    @Test
    fun `provider returns empty list by default`() = runBlocking {
        clipboardManager.clearPrimaryClip()

        val provider = ClipboardSuggestionProvider(testContext, mock())

        provider.onInputStarted()
        val suggestions = provider.onInputChanged("Hello")

        assertEquals(0, suggestions.size)
    }

    @Test
    fun `provider returns empty list for non plain text clip`() {
        clipboardManager.setPrimaryClip(
            ClipData.newHtmlText(
                "Label",
                "Hello mozilla.org",
                "<b>This is HTML on mozilla.org</b>"
            )
        )

        assertNull(getSuggestion())
    }

    @Test
    fun `provider should return suggestion if clipboard contains url`() {
        assertClipboardYieldsUrl(
            "https://www.mozilla.org",
            "https://www.mozilla.org")

        assertClipboardYieldsUrl(
            "https : //mozilla.org is a broken firefox.com URL",
            "mozilla.org")

        assertClipboardYieldsUrl(
            """
                This is a longer
                text over multiple lines
                and it https://www.mozilla.org contains
                URLs as well. https://www.firefox.com
            """,
            "https://www.mozilla.org")

        assertClipboardYieldsUrl(
        """
            mozilla.org
            firefox.com
            mozilla.org/en-US/firefox/developer/
            """,
            "mozilla.org")

        assertClipboardYieldsUrl("My IP is 192.168.0.1.", "192.168.0.1")
    }

    @Test
    fun `provider return suggestion on input start`() {
        clipboardManager.setPrimaryClip(ClipData.newPlainText("Test label", "https://www.mozilla.org"))

        val provider = ClipboardSuggestionProvider(testContext, mock())
        val suggestions = runBlocking { provider.onInputStarted() }

        assertEquals(1, suggestions.size)

        val suggestion = suggestions.firstOrNull()
        assertNotNull(suggestion!!)

        assertEquals("https://www.mozilla.org", suggestion.description)
    }

    @Test
    fun `provider should return no suggestions if clipboard does not contain a url`() {
        assertClipboardYieldsNothing("Hello World")

        assertClipboardYieldsNothing("Is this mozilla org")

        assertClipboardYieldsNothing("192.168.")
    }

    @Test
    fun `provider should allow customization of title and icon on suggestion`() {
        clipboardManager.setPrimaryClip(ClipData.newPlainText("Test label", "http://mozilla.org"))
        val bitmap = Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888)
        val provider = ClipboardSuggestionProvider(
            testContext,
            mock(),
            title = "My test title",
            icon = bitmap,
            requireEmptyText = false
        )

        val suggestion = runBlocking {
            provider.onInputStarted()
            val suggestions = provider.onInputChanged("Hello")

            suggestions.firstOrNull()
        }

        runBlocking {
            assertEquals(bitmap, suggestion?.icon?.invoke(2, 2))
            assertEquals("My test title", suggestion?.title)
        }
    }

    @Test
    fun `clicking suggestion loads url`() = runBlocking {
        clipboardManager.setPrimaryClip(
            ClipData.newPlainText(
                "Label",
                "Hello Mozilla, https://www.mozilla.org"
            )
        )

        val selectedEngineSession: EngineSession = mock()
        val selectedSession: Session = mock()
        val sessionManager: SessionManager = mock()
        `when`(sessionManager.selectedSession).thenReturn(selectedSession)
        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)

        val useCase = spy(SessionUseCases(sessionManager).loadUrl)

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
    fun `Provider suggestion should not get cleared when text changes`() {
        val provider = ClipboardSuggestionProvider(testContext, mock())
        assertFalse(provider.shouldClearSuggestions)
    }

    @Test
    fun `provider returns empty list for non-empty text if empty text required`() = runBlocking {
        clipboardManager.setPrimaryClip(
            ClipData.newPlainText(
                "Label",
                "Hello Mozilla, https://www.mozilla.org"
            )
        )

        val provider = ClipboardSuggestionProvider(testContext, mock(), requireEmptyText = true)
        val suggestions = provider.onInputChanged("Hello")
        assertTrue(suggestions.isEmpty())
    }

    private fun assertClipboardYieldsUrl(text: String, url: String) {
        val suggestion = getSuggestionWithClipboard(text)

        assertNotNull(suggestion)

        assertEquals(url, suggestion!!.description)
    }

    private fun assertClipboardYieldsNothing(text: String) {
        val suggestion = getSuggestionWithClipboard(text)
        assertNull(suggestion)
    }

    private fun getSuggestionWithClipboard(text: String): AwesomeBar.Suggestion? {
        clipboardManager.setPrimaryClip(ClipData.newPlainText("Test label", text))
        return getSuggestion()
    }

    private fun getSuggestion(): AwesomeBar.Suggestion? = runBlocking {
        val provider = ClipboardSuggestionProvider(testContext, mock(), requireEmptyText = false)

        provider.onInputStarted()
        val suggestions = provider.onInputChanged("Hello")

        suggestions.firstOrNull()
    }
}
