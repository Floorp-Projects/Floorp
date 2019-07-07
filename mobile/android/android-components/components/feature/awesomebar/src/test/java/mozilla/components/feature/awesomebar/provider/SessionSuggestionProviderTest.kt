/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class SessionSuggestionProviderTest {
    @Test
    fun `Provider returns empty list when text is empty`() = runBlocking {
        val provider = SessionSuggestionProvider(mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider returns Sessions with matching URLs`() = runBlocking {
        val sessionManager = SessionManager(mock())

        val provider = SessionSuggestionProvider(sessionManager, mock())

        run {
            val suggestions = provider.onInputChanged("Example")
            assertTrue(suggestions.isEmpty())
        }

        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://example.com"))
        sessionManager.add(Session("https://firefox.com"))

        run {
            val suggestions = provider.onInputChanged("Example")
            assertEquals(1, suggestions.size)

            assertEquals("https://example.com", suggestions[0].description)
        }

        sessionManager.add(Session("https://example.org/"))

        run {
            val suggestions = provider.onInputChanged("Example")
            assertEquals(2, suggestions.size)

            assertEquals("https://example.com", suggestions[0].description)
            assertEquals("https://example.org/", suggestions[1].description)
        }
    }

    @Test
    fun `Provider returns Sessions with matching titles`() = runBlocking {
        val sessionManager = SessionManager(mock())

        sessionManager.add(Session("https://allizom.org").apply {
            title = "Internet for people, not profit — Mozilla" })
        sessionManager.add(Session("https://getpocket.com").apply {
            title = "Pocket: My List" })
        sessionManager.add(Session("https://firefox.com").apply {
            title = "Download Firefox — Free Web Browser" })

        val provider = SessionSuggestionProvider(sessionManager, mock())

        run {
            val suggestions = provider.onInputChanged("Browser")
            assertEquals(1, suggestions.size)

            assertEquals("https://firefox.com", suggestions[0].description)
            assertEquals("Download Firefox — Free Web Browser", suggestions[0].title)
        }

        run {
            val suggestions = provider.onInputChanged("Mozilla")
            assertEquals(1, suggestions.size)

            assertEquals("https://allizom.org", suggestions[0].description)
            assertEquals("Internet for people, not profit — Mozilla", suggestions[0].title)
        }
    }

    @Test
    fun `Provider only returns non-private Sessions`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val session = Session("https://www.mozilla.org")
        val privateSession1 = Session("https://mozilla.org/firefox", true)
        val privateSession2 = Session("https://mozilla.org/projects", true)
        sessionManager.add(privateSession1)
        sessionManager.add(session)
        sessionManager.add(privateSession2)

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(sessionManager, useCase)
        val suggestions = provider.onInputChanged("mozilla")

        assertEquals(1, suggestions.size)
    }

    @Test
    fun `Clicking suggestion invokes SelectTabUseCase`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val session = Session("https://www.mozilla.org")
        sessionManager.add(session)

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(sessionManager, useCase)
        val suggestions = provider.onInputChanged("mozilla")
        assertEquals(1, suggestions.size)

        val suggestion = suggestions[0]

        verify(useCase, never()).invoke(session)

        suggestion.onSuggestionClicked!!.invoke()

        verify(useCase).invoke(session)
    }

    @Test
    fun `Provider suggestion should get cleared when text changes`() {
        val provider = SessionSuggestionProvider(mock(), mock())
        assertTrue(provider.shouldClearSuggestions)
    }

    @Test
    fun `When excludeSelectedSession is true provider should not include the selected session`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val session = Session("https://wikipedia.org")
        val selectedSession = Session("https://www.mozilla.org")

        sessionManager.add(selectedSession)
        sessionManager.add(session)
        sessionManager.select(selectedSession)

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(sessionManager, useCase, excludeSelectedSession = true)
        val suggestions = provider.onInputChanged("org")

        assertEquals(1, suggestions.size)
        assertEquals(session.url, suggestions.first().description)
    }

    @Test
    fun `When excludeSelectedSession is false provider should include the selected session`() = runBlocking {
        val sessionManager = SessionManager(mock())
        val session = Session("https://wikipedia.org")
        val selectedSession = Session("https://www.mozilla.org")

        sessionManager.add(selectedSession)
        sessionManager.add(session)
        sessionManager.select(selectedSession)

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(sessionManager, useCase, excludeSelectedSession = false)
        val suggestions = provider.onInputChanged("mozilla")

        assertEquals(1, suggestions.size)
        assertEquals(selectedSession.url, suggestions.first().description)
    }
}
