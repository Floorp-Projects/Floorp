/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.res.Resources
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

class SessionSuggestionProviderTest {
    @Test
    fun `Provider returns empty list when text is empty`() = runBlocking {
        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val provider = SessionSuggestionProvider(resources, mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider returns Sessions with matching URLs`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val resources: Resources = mock()
        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://example.com")
        val session3 = Session("https://firefox.com")
        val session4 = Session("https://example.org/")

        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val provider = SessionSuggestionProvider(resources, sessionManager, mock())

        run {
            val suggestions = provider.onInputChanged("Example")
            assertTrue(suggestions.isEmpty())
        }

        sessionManager.add(session1)
        sessionManager.add(session2)
        sessionManager.add(session3)

        run {
            val suggestions = provider.onInputChanged("Example")
            assertEquals(1, suggestions.size)

            assertEquals(session2.id, suggestions[0].id)
            assertEquals("Switch to tab", suggestions[0].description)
        }

        sessionManager.add(session4)

        run {
            val suggestions = provider.onInputChanged("Example")
            assertEquals(2, suggestions.size)

            assertEquals(session2.id, suggestions[0].id)
            assertEquals(session4.id, suggestions[1].id)
            assertEquals("Switch to tab", suggestions[0].description)
            assertEquals("Switch to tab", suggestions[1].description)
        }
    }

    @Test
    fun `Provider returns Sessions with matching titles`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val resources: Resources = mock()
        val session1 = Session("https://allizom.org").apply {
            title = "Internet for people, not profit — Mozilla" }
        val session2 = Session("https://getpocket.com").apply {
            title = "Pocket: My List" }
        val session3 = Session("https://firefox.com").apply {
            title = "Download Firefox — Free Web Browser" }

        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        sessionManager.add(session1)
        sessionManager.add(session2)
        sessionManager.add(session3)

        val provider = SessionSuggestionProvider(resources, sessionManager, mock())

        run {
            val suggestions = provider.onInputChanged("Browser")
            assertEquals(1, suggestions.size)

            assertEquals(suggestions.first().id, session3.id)
            assertEquals("Switch to tab", suggestions.first().description)
            assertEquals("Download Firefox — Free Web Browser", suggestions[0].title)
        }

        run {
            val suggestions = provider.onInputChanged("Mozilla")
            assertEquals(1, suggestions.size)

            assertEquals(session1.id, suggestions.first().id)
            assertEquals("Switch to tab", suggestions.first().description)
            assertEquals("Internet for people, not profit — Mozilla", suggestions[0].title)
        }
    }

    @Test
    fun `Provider only returns non-private Sessions`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val session = Session("https://www.mozilla.org")
        val privateSession1 = Session("https://mozilla.org/firefox", true)
        val privateSession2 = Session("https://mozilla.org/projects", true)
        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")
        sessionManager.add(privateSession1)
        sessionManager.add(session)
        sessionManager.add(privateSession2)

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, sessionManager, useCase)
        val suggestions = provider.onInputChanged("mozilla")

        assertEquals(1, suggestions.size)
    }

    @Test
    fun `Clicking suggestion invokes SelectTabUseCase`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val session = Session("https://www.mozilla.org")
        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")
        sessionManager.add(session)

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, sessionManager, useCase)
        val suggestions = provider.onInputChanged("mozilla")
        assertEquals(1, suggestions.size)

        val suggestion = suggestions[0]

        verify(useCase, never()).invoke(session)

        suggestion.onSuggestionClicked!!.invoke()

        verify(useCase).invoke(session)
    }

    @Test
    fun `Provider suggestion should get cleared when text changes`() {
        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val provider = SessionSuggestionProvider(resources, mock(), mock())

        assertTrue(provider.shouldClearSuggestions)
    }

    @Test
    fun `When excludeSelectedSession is true provider should not include the selected session`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val session = Session("https://wikipedia.org")
        val selectedSession = Session("https://www.mozilla.org")
        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        sessionManager.add(selectedSession)
        sessionManager.add(session)
        sessionManager.select(selectedSession)

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, sessionManager, useCase, excludeSelectedSession = true)
        val suggestions = provider.onInputChanged("org")

        assertEquals(1, suggestions.size)
        assertEquals(session.id, suggestions.first().id)
        assertEquals("Switch to tab", suggestions.first().description)
    }

    @Test
    fun `When excludeSelectedSession is false provider should include the selected session`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val session = Session("https://wikipedia.org")
        val selectedSession = Session("https://www.mozilla.org")
        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        sessionManager.add(selectedSession)
        sessionManager.add(session)
        sessionManager.select(selectedSession)

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, sessionManager, useCase, excludeSelectedSession = false)
        val suggestions = provider.onInputChanged("mozilla")

        assertEquals(1, suggestions.size)
        assertEquals(selectedSession.id, suggestions.first().id)
        assertEquals("Switch to tab", suggestions.first().description)
    }
}
