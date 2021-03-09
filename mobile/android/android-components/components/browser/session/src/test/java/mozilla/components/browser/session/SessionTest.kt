/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.SessionState.Source
import mozilla.components.browser.session.ext.toSecurityInfoState
import mozilla.components.browser.session.ext.toTabSessionState
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
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
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@Suppress("DEPRECATION") // Session observable is deprecated
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
    fun `action is dispatched when URL changes`() {
        val store: BrowserStore = mock()
        `when`(store.dispatch(any())).thenReturn(mock())

        val session = Session("https://www.mozilla.org")
        session.store = store
        session.url = "http://www.firefox.com"

        verify(store).dispatch(ContentAction.UpdateUrlAction(session.id, session.url))
        verifyNoMoreInteractions(store)
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
    fun `action is dispatched when progress changes`() {
        val store: BrowserStore = mock()
        `when`(store.dispatch(any())).thenReturn(mock())

        val session = Session("https://www.mozilla.org")
        session.store = store
        session.progress = 75

        verify(store).dispatch(ContentAction.UpdateProgressAction(session.id, session.progress))
        verifyNoMoreInteractions(store)
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
    fun `action is dispatched when loading state changes`() {
        val store: BrowserStore = mock()
        `when`(store.dispatch(any())).thenReturn(mock())

        val session = Session("https://www.mozilla.org")
        session.store = store
        session.loading = true

        verify(store).dispatch(ContentAction.UpdateLoadingStateAction(session.id, session.loading))
        verifyNoMoreInteractions(store)
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
    fun `action is dispatched when security info is set`() {
        val store: BrowserStore = mock()
        `when`(store.dispatch(any())).thenReturn(mock())

        val session = Session("https://www.mozilla.org")
        session.store = store
        session.securityInfo = Session.SecurityInfo(true, "mozilla.org", "issuer")

        verify(store).dispatch(ContentAction.UpdateSecurityInfoAction(session.id, session.securityInfo.toSecurityInfoState()))
        verifyNoMoreInteractions(store)
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

        val customTabConfig = CustomTabConfig(
            toolbarColor = null,
            closeButtonIcon = null,
            enableUrlbarHiding = true,
            actionButtonConfig = null,
            showShareMenuItem = true
        )
        session.customTabConfig = customTabConfig

        assertEquals(customTabConfig, session.customTabConfig)

        assertNotNull(config)
    }

    @Test
    fun `action is dispatched when custom tab config is set`() {
        val store: BrowserStore = mock()
        `when`(store.dispatch(any())).thenReturn(mock())

        val session = Session("https://www.mozilla.org")
        session.store = store
        session.customTabConfig = mock()

        verify(store, never()).dispatch(CustomTabListAction.RemoveCustomTabAction(session.id))
        verify(store, never()).dispatch(TabListAction.AddTabAction(session.toTabSessionState()))

        session.customTabConfig = null
        verify(store).dispatch(CustomTabListAction.TurnCustomTabIntoNormalTabAction(session.id))
        verifyNoMoreInteractions(store)
    }

    @Test
    fun `observer is notified when web app manifest is set`() {
        val manifest = WebAppManifest(
            name = "HackerWeb",
            description = "A simply readable Hacker News app.",
            startUrl = ".",
            display = WebAppManifest.DisplayMode.STANDALONE,
            icons = listOf(
                WebAppManifest.Icon(
                    src = "images/touch/homescreen192.png",
                    sizes = listOf(Size(192, 192)),
                    type = "image/png"
                )
            )
        )

        val observer: Session.Observer = mock()

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        assertNull(session.webAppManifest)

        session.webAppManifest = manifest
        assertEquals(manifest, session.webAppManifest)

        val captor = argumentCaptor<WebAppManifest>()
        verify(observer).onWebAppManifestChanged(eq(session), captor.capture())
        assertEquals(manifest, captor.value)
    }

    @Test
    fun `action is dispatched when web app manifest is set`() {
        val manifest = WebAppManifest(
            name = "HackerWeb",
            description = "A simply readable Hacker News app.",
            startUrl = ".",
            display = WebAppManifest.DisplayMode.STANDALONE,
            icons = listOf(
                WebAppManifest.Icon(
                    src = "images/touch/homescreen192.png",
                    sizes = listOf(Size(192, 192)),
                    type = "image/png"
                )
            )
        )

        val store: BrowserStore = mock()
        `when`(store.dispatch(any())).thenReturn(mock())

        val session = Session("https://www.mozilla.org")
        session.store = store

        session.webAppManifest = manifest
        verify(store).dispatch(ContentAction.UpdateWebAppManifestAction(session.id, manifest))

        session.webAppManifest = null
        verify(store).dispatch(ContentAction.RemoveWebAppManifestAction(session.id))

        verifyNoMoreInteractions(store)
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

        session = Session("https://www.mozilla.org", false, Source.NONE, "s1")
        assertNotNull(session.id)
        assertEquals("s1", session.id)
    }

    @Test
    fun `sessions with the same id are equal`() {
        val session1 = Session("http://mozilla.org", false, Source.NONE, "123ABC")
        val session2 = Session("http://mozilla.org", false, Source.NONE, "123ABC")

        assertEquals(session1, session2)
    }

    @Test
    fun `session ID is used for hashCode`() {
        val session1 = Session("http://mozilla.org", false, Source.NONE, "123ABC")
        val session2 = Session("http://mozilla.org", false, Source.NONE, "123ABC")

        val map = mapOf(session1 to "test")
        assertEquals("test", map[session2])
        assertEquals(session1.hashCode(), session2.hashCode())
    }

    @Test
    fun `session returns context ID`() {
        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://www.mozilla.org", contextId = "1")
        val session3 = Session("https://www.mozilla.org", contextId = "2")

        assertNull(session1.contextId)
        assertEquals("1", session2.contextId)
        assertEquals("2", session3.contextId)
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
    fun `action is dispatched when title changes`() {
        val store: BrowserStore = mock()
        `when`(store.dispatch(any())).thenReturn(mock())

        val session = Session("https://www.mozilla.org")
        session.store = store
        session.title = "Internet for people, not profit — Mozilla"

        verify(store).dispatch(ContentAction.UpdateTitleAction(session.id, session.title))
        verifyNoMoreInteractions(store)
    }

    @Test
    fun `website title is empty by default`() {
        val session = Session("https://www.mozilla.org")
        assertTrue(session.title.isEmpty())
        assertEquals("", session.title)
    }

    @Test
    fun `observer is notified when tracker is blocked`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        val tracker1 = Tracker("trackerUrl1")
        val tracker2 = Tracker("trackerUrl2")

        session.trackersBlocked += tracker1

        verify(observer).onTrackerBlocked(
                eq(session),
                eq(tracker1),
                eq(listOf(tracker1)))

        assertEquals(listOf(tracker1), session.trackersBlocked)

        session.trackersBlocked += tracker2

        verify(observer).onTrackerBlocked(
                eq(session),
                eq(tracker2),
                eq(listOf(tracker1, tracker2)))

        assertEquals(listOf(tracker1, tracker2), session.trackersBlocked)

        session.trackersBlocked = emptyList()
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when a tracker is loaded`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        val tracker1 = Tracker("trackerUrl1")
        val tracker2 = Tracker("trackerUrl2")

        session.trackersLoaded += tracker1

        verify(observer).onTrackerLoaded(
            eq(session),
            eq(tracker1),
            eq(listOf(tracker1))
        )

        assertEquals(listOf(tracker1), session.trackersLoaded)

        session.trackersLoaded += tracker2

        verify(observer).onTrackerLoaded(
            eq(session),
            eq(tracker2),
            eq(listOf(tracker1, tracker2))
        )

        assertEquals(listOf(tracker1, tracker2), session.trackersLoaded)

        session.trackersLoaded = emptyList()
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when tracker blocking is enabled`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.trackerBlockingEnabled = true

        verify(observer).onTrackerBlockingEnabledChanged(
                eq(session),
                eq(true))

        assertTrue(session.trackerBlockingEnabled)
    }

    @Test
    fun `session observer has default methods`() {
        val session = Session("")
        val defaultObserver = object : Session.Observer {}
        val contentPermissionRequest: PermissionRequest = mock()

        defaultObserver.onUrlChanged(session, "")
        defaultObserver.onTitleChanged(session, "")
        defaultObserver.onProgress(session, 0)
        defaultObserver.onLoadingStateChanged(session, true)
        defaultObserver.onLoadRequest(session, "https://www.mozilla.org", true, true)
        defaultObserver.onSecurityChanged(session, Session.SecurityInfo())
        defaultObserver.onCustomTabConfigChanged(session, null)
        defaultObserver.onTrackerBlockingEnabledChanged(session, true)
        defaultObserver.onTrackerBlocked(session, mock(), emptyList())
        defaultObserver.onContentPermissionRequested(session, contentPermissionRequest)
        defaultObserver.onWebAppManifestChanged(session, mock())
        defaultObserver.onRecordingDevicesChanged(session, emptyList())
    }

    @Test
    fun `handle empty blocked trackers list race conditions`() {
        val observer = mock(Session.Observer::class.java)
        val observer2 = mock(Session.Observer::class.java)

        val session = Session("about:blank")
        session.register(observer)
        session.register(observer2)

        runBlocking {
            (1..3).map {
                val def = GlobalScope.async(IO) {
                    session.trackersBlocked = emptyList()
                    session.trackersBlocked += Tracker("test")
                    session.trackersBlocked = emptyList()
                }
                val def2 = GlobalScope.async(IO) {
                    session.trackersBlocked = emptyList()
                    session.trackersBlocked += Tracker("test")
                    session.trackersBlocked = emptyList()
                }
                def.await()
                def2.await()
            }
        }
    }

    @Test
    fun `handle empty loaded trackers list race conditions`() {
        val observer = mock(Session.Observer::class.java)
        val observer2 = mock(Session.Observer::class.java)

        val session = Session("about:blank")
        session.register(observer)
        session.register(observer2)

        runBlocking {
            (1..3).map {
                val def = GlobalScope.async(IO) {
                    session.trackersLoaded = emptyList()
                    session.trackersLoaded += Tracker("test")
                    session.trackersLoaded = emptyList()
                }
                val def2 = GlobalScope.async(IO) {
                    session.trackersLoaded = emptyList()
                    session.trackersLoaded += Tracker("test")
                    session.trackersLoaded = emptyList()
                }
                def.await()
                def2.await()
            }
        }
    }

    @Test
    fun `toString returns string containing id and url`() {
        val session = Session(id = "my-session-id", initialUrl = "https://www.mozilla.org")
        assertEquals("Session(my-session-id, https://www.mozilla.org)", session.toString())
    }

    @Test
    fun `hasParentSession returns false by default`() {
        val session = Session("https://www.mozilla.org")
        assertFalse(session.hasParentSession)
    }

    @Test
    fun `hasParentSession returns true if added to SessionManager with a parent`() {
        val sessionManager = SessionManager(engine = mock())

        val parentSession = Session("https://www.mozilla.org")
        sessionManager.add(parentSession)

        val session = Session("https://www.mozilla.org/en-US/firefox/accounts/")

        assertFalse(parentSession.hasParentSession)
        assertFalse(session.hasParentSession)

        sessionManager.add(session, parent = parentSession)

        assertFalse(parentSession.hasParentSession)
        assertTrue(session.hasParentSession)
    }
}
