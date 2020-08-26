/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.content.Intent
import android.os.Bundle
import android.provider.Browser
import androidx.browser.customtabs.CustomTabsIntent
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.SessionState.Source
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.intent.ext.EXTRA_SESSION_ID
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import mozilla.components.support.utils.toSafeIntent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class CustomTabIntentProcessorTest {

    @Test
    fun processCustomTabIntentWithDefaultHandlers() {
        val store: BrowserStore = mock()
        val engine: Engine = mock()
        val sessionManager = spy(SessionManager(engine))
        val useCases = SessionUseCases(store, sessionManager)

        val handler =
            CustomTabIntentProcessor(sessionManager, useCases.loadUrl, testContext.resources)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)
        whenever(intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)).thenReturn(true)
        whenever(intent.dataString).thenReturn("http://mozilla.org")
        whenever(intent.putExtra(any<String>(), any<String>())).thenReturn(intent)

        handler.process(intent)

        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(false), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, "http://mozilla.org", LoadUrlFlags.external())
        )
        verify(intent).putExtra(eq(EXTRA_SESSION_ID), any<String>())

        val customTabSession = sessionManager.all[0]
        assertNotNull(customTabSession)
        assertEquals("http://mozilla.org", customTabSession.url)
        assertEquals(Source.CUSTOM_TAB, customTabSession.source)
        assertNotNull(customTabSession.customTabConfig)
        assertFalse(customTabSession.private)
    }

    @Test
    fun processCustomTabIntentWithAdditionalHeaders() {
        val store: BrowserStore = mock()
        val engine: Engine = mock()
        val sessionManager = spy(SessionManager(engine))
        val useCases = SessionUseCases(store, sessionManager)

        val handler =
                CustomTabIntentProcessor(sessionManager, useCases.loadUrl, testContext.resources)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)
        whenever(intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)).thenReturn(true)
        whenever(intent.dataString).thenReturn("http://mozilla.org")
        whenever(intent.putExtra(any<String>(), any<String>())).thenReturn(intent)

        val headersBundle = Bundle().apply {
            putString("X-Extra-Header", "true")
        }
        whenever(intent.getBundleExtra(Browser.EXTRA_HEADERS)).thenReturn(headersBundle)
        val headers = handler.getAdditionalHeaders(intent.toSafeIntent())

        handler.process(intent)

        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(false), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, "http://mozilla.org", LoadUrlFlags.external(), headers)
        )
        verify(intent).putExtra(eq(EXTRA_SESSION_ID), any<String>())

        val customTabSession = sessionManager.all[0]
        assertNotNull(customTabSession)
        assertEquals("http://mozilla.org", customTabSession.url)
        assertEquals(Source.CUSTOM_TAB, customTabSession.source)
        assertNotNull(customTabSession.customTabConfig)
        assertFalse(customTabSession.private)
    }

    @Test
    fun processPrivateCustomTabIntentWithDefaultHandlers() {
        val store: BrowserStore = mock()
        val engine: Engine = mock()
        val sessionManager = spy(SessionManager(engine))
        val useCases = SessionUseCases(store, sessionManager)

        val handler =
                CustomTabIntentProcessor(sessionManager, useCases.loadUrl, testContext.resources, true)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)
        whenever(intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)).thenReturn(true)
        whenever(intent.dataString).thenReturn("http://mozilla.org")
        whenever(intent.putExtra(any<String>(), any<String>())).thenReturn(intent)

        handler.process(intent)

        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(false), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, "http://mozilla.org", LoadUrlFlags.external())
        )
        verify(intent).putExtra(eq(EXTRA_SESSION_ID), any<String>())

        val customTabSession = sessionManager.all[0]
        assertNotNull(customTabSession)
        assertEquals("http://mozilla.org", customTabSession.url)
        assertEquals(Source.CUSTOM_TAB, customTabSession.source)
        assertNotNull(customTabSession.customTabConfig)
        assertTrue(customTabSession.private)
    }
}
