/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.utils.observer.Consumable
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class SessionTest {
    @Test
    fun `registered observers get notified`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.notifyObservers { onUrlChanged(session, "https://getpocket.com") }

        verify(observer).onUrlChanged(eq(session), eq("https://getpocket.com"))
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is not notified after unregistering`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.notifyObservers { onUrlChanged(session, "https://getpocket.com") }

        verify(observer).onUrlChanged(session, "https://getpocket.com")
        verifyNoMoreInteractions(observer)

        reset(observer)

        session.unregister(observer)
        session.notifyObservers { onUrlChanged(session, "https://www.firefox.com") }

        verify(observer, never()).onUrlChanged(any(), any())
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when URL changes`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.url = "http://www.firefox.com"

        assertEquals("http://www.firefox.com", session.url)
        verify(observer).onUrlChanged(eq(session), eq("http://www.firefox.com"))
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when progress changes`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.progress = 75

        assertEquals(75, session.progress)
        verify(observer).onProgress(eq(session), eq(75))
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when loading state changes`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.loading = true

        assertEquals(true, session.loading)
        verify(observer).onLoadingStateChanged(eq(session), eq(true))
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when navigation state changes`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.canGoBack = true
        assertEquals(true, session.canGoBack)
        verify(observer).onNavigationStateChanged(eq(session), eq(true), eq(false))

        session.canGoForward = true
        assertEquals(true, session.canGoForward)
        verify(observer).onNavigationStateChanged(eq(session), eq(true), eq(true))

        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is not notified when property is set but hasn't changed`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.url = "https://www.mozilla.org"

        assertEquals("https://www.mozilla.org", session.url)
        verify(observer, never()).onUrlChanged(eq(session), eq("https://www.mozilla.org"))
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when search terms are set`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.searchTerms = ""
        session.searchTerms = "mozilla android"

        assertEquals("mozilla android", session.searchTerms)
        verify(observer, times(1)).onSearch(eq(session), eq("mozilla android"))
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when security info is set`() {
        var info: Session.SecurityInfo? = null

        val observer = object : Session.Observer {
            override fun onSecurityChanged(session: Session, securityInfo: Session.SecurityInfo) {
                info = securityInfo
            }
        }

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.securityInfo = Session.SecurityInfo(true, "mozilla.org", "issuer")

        assertEquals(Session.SecurityInfo(true, "mozilla.org", "issuer"),
            session.securityInfo)

        assertNotNull(info)
        assertEquals(true, info!!.secure)
        assertEquals("mozilla.org", info!!.host)
        assertEquals("issuer", info!!.issuer)
    }

    @Test
    fun `observer is notified when custom tab config is set`() {
        var config: CustomTabConfig? = null
        val observer = object : Session.Observer {
            override fun onCustomTabConfigChanged(session: Session, customTabConfig: CustomTabConfig?) {
                config = customTabConfig
            }
        }

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        assertNull(session.customTabConfig)

        val customTabConfig = CustomTabConfig("id", null, null, true, null, true, listOf(), listOf())
        session.customTabConfig = customTabConfig

        assertEquals(customTabConfig, session.customTabConfig)

        assertNotNull(config)
        assertEquals("id", config!!.id)
    }

    @Test
    fun `session returns initial URL`() {
        val session = Session("https://www.mozilla.org")

        assertEquals("https://www.mozilla.org", session.url)
    }

    @Test
    fun `session always has an ID`() {
        var session = Session("https://www.mozilla.org")
        assertNotNull(session.id)

        session = Session("https://www.mozilla.org", Source.NONE, "s1")
        assertNotNull(session.id)
        assertEquals("s1", session.id)
    }

    @Test
    fun `sessions with the same id are equal`() {
        val session1 = Session("http://mozilla.org", Source.NONE, "123ABC")
        val session2 = Session("http://mozilla.org", Source.NONE, "123ABC")

        assertEquals(session1, session2)
    }

    @Test
    fun `session ID is used for hashCode`() {
        val session1 = Session("http://mozilla.org", Source.NONE, "123ABC")
        val session2 = Session("http://mozilla.org", Source.NONE, "123ABC")

        val map = mapOf(session1 to "test")
        assertEquals("test", map[session2])
        assertEquals(session1.hashCode(), session2.hashCode())
    }

    @Test
    fun `Download will be set on Session if no observer is registered`() {
        val download: Download = mock()
        `when`(download.url).thenReturn("https://download.mozilla.org/")

        val session = Session("https://www.mozilla.org")
        session.download = Consumable.from(download)

        assertFalse(session.download.isConsumed())

        var downloadIsSet = false

        session.download.consume { consumable ->
            downloadIsSet = consumable.url == "https://download.mozilla.org/"
            true
        }

        assertTrue(downloadIsSet)
    }

    @Test
    fun `Download will not be set on Session if consumed by observer`() {
        var callbackExecuted = false

        val session = Session("https://www.mozilla.org")
        session.register(object : Session.Observer {
            override fun onDownload(session: Session, download: Download): Boolean {
                callbackExecuted = true
                return true // Consume download
            }
        })

        val download: Download = mock()
        session.download = Consumable.from(download)

        assertTrue(callbackExecuted)
        assertTrue(session.download.isConsumed())
    }

    @Test
    fun `All observers will not be notified about a download`() {
        var firstCallbackExecuted = false
        var secondCallbackExecuted = false

        val session = Session("https://www.mozilla.org")
        session.register(object : Session.Observer {
            override fun onDownload(session: Session, download: Download): Boolean {
                firstCallbackExecuted = true
                return true // Consume download
            }
        })
        session.register(object : Session.Observer {
            override fun onDownload(session: Session, download: Download): Boolean {
                secondCallbackExecuted = true
                return false // Do not consume download
            }
        })

        val download: Download = mock()
        session.download = Consumable.from(download)

        assertTrue(firstCallbackExecuted)
        assertTrue(secondCallbackExecuted)
        assertTrue(session.download.isConsumed())
    }

    @Test
    fun `Download will be set on Session if no observer consumes it`() {
        var firstCallbackExecuted = false
        var secondCallbackExecuted = false

        val session = Session("https://www.mozilla.org")
        session.register(object : Session.Observer {
            override fun onDownload(session: Session, download: Download): Boolean {
                firstCallbackExecuted = true
                return false // Do not consume download
            }
        })
        session.register(object : Session.Observer {
            override fun onDownload(session: Session, download: Download): Boolean {
                secondCallbackExecuted = true
                return false // Do not consume download
            }
        })

        val download: Download = mock()
        session.download = Consumable.from(download)

        assertTrue(firstCallbackExecuted)
        assertTrue(secondCallbackExecuted)
        assertFalse(session.download.isConsumed())
    }

    @Test
    fun `Download can be consumed`() {
        val session = Session("https://www.mozilla.org")
        session.download = Consumable.from(mock())

        assertFalse(session.download.isConsumed())

        var consumerExecuted = false
        session.download.consume {
            consumerExecuted = true
            false // Do not consume
        }

        assertTrue(consumerExecuted)
        assertFalse(session.download.isConsumed())

        consumerExecuted = false
        session.download.consume {
            consumerExecuted = true
            true // Consume download
        }

        assertTrue(consumerExecuted)
        assertTrue(session.download.isConsumed())
    }

    @Test
    fun `observer is notified when title changes`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.title = "Internet for people, not profit — Mozilla"

        verify(observer).onTitleChanged(
            eq(session),
            eq("Internet for people, not profit — Mozilla"))

        assertEquals("Internet for people, not profit — Mozilla", session.title)
    }

    @Test
    fun `website title is empty by default`() {
        val session = Session("https://www.mozilla.org")
        assertTrue(session.title.isEmpty())
        assertEquals("", session.title)
    }
}
