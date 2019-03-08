/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import android.graphics.Bitmap
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.manifest.WebAppManifest
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
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
    fun `observer is notified when web app manifest is set`() {
        val manifest = WebAppManifest(
            name = "HackerWeb",
            description = "A simply readable Hacker News app.",
            startUrl = ".",
            display = WebAppManifest.DisplayMode.STANDALONE,
            icons = listOf(
                WebAppManifest.Icon(
                    src = "images/touch/homescreen192.png",
                    sizes = listOf(WebAppManifest.Icon.Size(192, 192)),
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
    fun `HitResult will be set on Session`() {
        val hitResult: HitResult = mock()
        `when`(hitResult.src).thenReturn("https://mozilla.org")

        val session = Session("http://firefox.com")
        session.hitResult = Consumable.from(hitResult)

        assertFalse(session.hitResult.isConsumed())

        var hitResultIsSet = false
        session.hitResult.consume { consumable ->
            hitResultIsSet = consumable.src == "https://mozilla.org"
            true
        }

        assertTrue(hitResultIsSet)
        assertTrue(session.hitResult.isConsumed())
    }

    @Test
    fun `HitResult will not be set on Session if consumed by observer`() {
        var callbackExecuted = false

        val session = Session("https://www.mozilla.org")
        session.register(object : Session.Observer {
            override fun onLongPress(session: Session, hitResult: HitResult): Boolean {
                callbackExecuted = true
                return true // Consume HitResult
            }
        })

        val hitResult: HitResult = mock()
        session.hitResult = Consumable.from(hitResult)

        assertTrue(callbackExecuted)
        assertTrue(session.hitResult.isConsumed())
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

    @Test
    fun `observer is notified when tracker is blocked`() {
        val observer = mock(Session.Observer::class.java)

        val session = Session("https://www.mozilla.org")
        session.register(observer)

        session.trackersBlocked += "trackerUrl1"

        verify(observer).onTrackerBlocked(
                eq(session),
                eq("trackerUrl1"),
                eq(listOf("trackerUrl1")))

        assertEquals(listOf("trackerUrl1"), session.trackersBlocked)

        session.trackersBlocked += "trackerUrl2"

        verify(observer).onTrackerBlocked(
                eq(session),
                eq("trackerUrl2"),
                eq(listOf("trackerUrl1", "trackerUrl2")))

        assertEquals(listOf("trackerUrl1", "trackerUrl2"), session.trackersBlocked)

        session.trackersBlocked = emptyList()
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
    fun `observer is notified on find result`() {
        val observer = mock(Session.Observer::class.java)
        val session = Session("https://www.mozilla.org")
        session.register(observer)

        val result1 = Session.FindResult(0, 1, false)
        session.findResults += result1
        verify(observer).onFindResult(
                eq(session),
                eq(result1)
        )
        assertEquals(listOf(result1), session.findResults)

        result1.copy()
        val result2 = result1.copy(1, 2)
        session.findResults += result2
        verify(observer).onFindResult(
                eq(session),
                eq(result2)
        )
        assertEquals(listOf(result1, result2), session.findResults)

        assertEquals(session.findResults[0].activeMatchOrdinal, 0)
        assertEquals(session.findResults[0].numberOfMatches, 1)
        assertFalse(session.findResults[0].isDoneCounting)
        assertEquals(session.findResults[1].activeMatchOrdinal, 1)
        assertEquals(session.findResults[1].numberOfMatches, 2)
        assertFalse(session.findResults[1].isDoneCounting)
        assertNotEquals(result1, result2)

        session.findResults = emptyList()
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is notified when desktop mode is set`() {
        val observer = mock(Session.Observer::class.java)
        val session = Session("https://www.mozilla.org")
        session.register(observer)
        session.desktopMode = true
        verify(observer).onDesktopModeChanged(
                eq(session),
                eq(true))
        assertTrue(session.desktopMode)
    }

    @Test
    fun `observer is notified on fullscreen mode`() {
        val observer = mock(Session.Observer::class.java)
        val session = Session("https://www.mozilla.org")
        session.register(observer)
        session.fullScreenMode = true
        verify(observer).onFullScreenChanged(session, true)
        reset(observer)
        session.unregister(observer)
        session.fullScreenMode = false
        verify(observer, never()).onFullScreenChanged(session, false)
    }

    @Test
    fun `observer is notified on on thumbnail changed `() {
        val observer = mock(Session.Observer::class.java)
        val session = Session("https://www.mozilla.org")
        val emptyThumbnail = spy(Bitmap::class.java)
        session.register(observer)
        session.thumbnail = emptyThumbnail
        verify(observer).onThumbnailChanged(session, emptyThumbnail)
        reset(observer)
        session.unregister(observer)
        session.thumbnail = emptyThumbnail
        verify(observer, never()).onThumbnailChanged(session, emptyThumbnail)
    }

    @Test
    fun `session observer has default methods`() {
        val session = Session("")
        val defaultObserver = object : Session.Observer {}
        val contentPermissionRequest: PermissionRequest = mock()
        val appPermissionRequest: PermissionRequest = mock()
        val promptRequest: PromptRequest = mock()
        val windowRequest: WindowRequest = mock()

        defaultObserver.onUrlChanged(session, "")
        defaultObserver.onTitleChanged(session, "")
        defaultObserver.onProgress(session, 0)
        defaultObserver.onLoadingStateChanged(session, true)
        defaultObserver.onNavigationStateChanged(session, true, true)
        defaultObserver.onSearch(session, "")
        defaultObserver.onSecurityChanged(session, Session.SecurityInfo())
        defaultObserver.onCustomTabConfigChanged(session, null)
        defaultObserver.onDownload(session, mock(Download::class.java))
        defaultObserver.onTrackerBlockingEnabledChanged(session, true)
        defaultObserver.onTrackerBlocked(session, "", emptyList())
        defaultObserver.onLongPress(session, mock(HitResult::class.java))
        defaultObserver.onFindResult(session, mock(Session.FindResult::class.java))
        defaultObserver.onDesktopModeChanged(session, true)
        defaultObserver.onFullScreenChanged(session, true)
        defaultObserver.onThumbnailChanged(session, spy(Bitmap::class.java))
        defaultObserver.onContentPermissionRequested(session, contentPermissionRequest)
        defaultObserver.onAppPermissionRequested(session, appPermissionRequest)
        defaultObserver.onPromptRequested(session, promptRequest)
        defaultObserver.onOpenWindowRequested(session, windowRequest)
        defaultObserver.onCloseWindowRequested(session, windowRequest)
        defaultObserver.onWebAppManifestChanged(session, mock())
    }

    @Test
    fun `permission requests will be set on session if no observer consumes them`() {
        val contentPermissionRequest: PermissionRequest = mock()
        val appPermissionRequest: PermissionRequest = mock()

        val session = Session("https://www.mozilla.org")
        session.contentPermissionRequest = Consumable.from(contentPermissionRequest)
        session.appPermissionRequest = Consumable.from(appPermissionRequest)
        assertFalse(session.contentPermissionRequest.isConsumed())

        var contentPermissionRequestIsSet = false
        var appPermissionRequestIsSet = false
        session.contentPermissionRequest.consume {
            contentPermissionRequestIsSet = true
            true
        }
        session.appPermissionRequest.consume {
            appPermissionRequestIsSet = true
            true
        }
        assertTrue(contentPermissionRequestIsSet)
        assertTrue(appPermissionRequestIsSet)
    }

    @Test
    fun `permission requests will not be set on session if consumed by observer`() {
        var contentPermissionCallbackExecuted = false
        var appPermissionCallbackExecuted = false

        val session = Session("https://www.mozilla.org")
        session.register(object : Session.Observer {
            override fun onContentPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
                contentPermissionCallbackExecuted = true
                return true
            }

            override fun onAppPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
                appPermissionCallbackExecuted = true
                return true
            }
        })

        val contentPermissionRequest: PermissionRequest = mock()
        session.contentPermissionRequest = Consumable.from(contentPermissionRequest)

        val appPermissionRequestIsSet: PermissionRequest = mock()
        session.appPermissionRequest = Consumable.from(appPermissionRequestIsSet)

        assertTrue(contentPermissionCallbackExecuted)
        assertTrue(session.contentPermissionRequest.isConsumed())

        assertTrue(appPermissionCallbackExecuted)
        assertTrue(session.appPermissionRequest.isConsumed())
    }

    @Test
    fun `prompt requests will be set on session if no observer consumes it`() {
        val promptRequest: PromptRequest = mock()

        val session = Session("https://www.mozilla.org")
        session.promptRequest = Consumable.from(promptRequest)
        assertFalse(session.promptRequest.isConsumed())

        var promptRequestRequestIsSet = false
        session.promptRequest.consume {
            promptRequestRequestIsSet = true
            true
        }

        assertTrue(promptRequestRequestIsSet)
    }

    @Test
    fun `prompt requests will not be set on session if consumed by observer`() {
        var promptCallbackExecuted = false

        val session = Session("https://www.mozilla.org")
        session.register(object : Session.Observer {
            override fun onPromptRequested(session: Session, promptRequest: PromptRequest): Boolean {
                promptCallbackExecuted = true
                return true
            }
        })

        val promptRequest: PromptRequest = mock()
        session.promptRequest = Consumable.from(promptRequest)

        assertTrue(promptCallbackExecuted)
        assertTrue(session.contentPermissionRequest.isConsumed())
    }

    @Test
    fun `window requests will be set on session if no observer consumes them`() {
        val openWindowRequest: WindowRequest = mock()
        val closeWindowRequest: WindowRequest = mock()

        val session = Session("https://www.mozilla.org")
        session.openWindowRequest = Consumable.from(openWindowRequest)
        session.closeWindowRequest = Consumable.from(closeWindowRequest)
        assertFalse(session.openWindowRequest.isConsumed())
        assertFalse(session.closeWindowRequest.isConsumed())

        var createWindowRequestIsSet = false
        var closeWindowRequestIsSet = false
        session.openWindowRequest.consume {
            createWindowRequestIsSet = true
            true
        }
        session.closeWindowRequest.consume {
            closeWindowRequestIsSet = true
            true
        }
        assertTrue(createWindowRequestIsSet)
        assertTrue(closeWindowRequestIsSet)
    }

    @Test
    fun `window requests will not be set on session if consumed by observer`() {
        var openWindowRequestExecuted = false
        var closeWindowRequestExecuted = false

        val session = Session("https://www.mozilla.org")
        session.register(object : Session.Observer {
            override fun onOpenWindowRequested(session: Session, windowRequest: WindowRequest): Boolean {
                openWindowRequestExecuted = true
                return true
            }

            override fun onCloseWindowRequested(session: Session, windowRequest: WindowRequest): Boolean {
                closeWindowRequestExecuted = true
                return true
            }
        })

        val createWindowRequest: WindowRequest = mock()
        session.openWindowRequest = Consumable.from(createWindowRequest)

        val closeWindowRequest: WindowRequest = mock()
        session.closeWindowRequest = Consumable.from(closeWindowRequest)

        assertTrue(openWindowRequestExecuted)
        assertTrue(session.openWindowRequest.isConsumed())

        assertTrue(closeWindowRequestExecuted)
        assertTrue(session.closeWindowRequest.isConsumed())
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
                    session.trackersBlocked += "test"
                    session.trackersBlocked = emptyList()
                }
                val def2 = GlobalScope.async(IO) {
                    session.trackersBlocked = emptyList()
                    session.trackersBlocked += "test"
                    session.trackersBlocked = emptyList()
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
}
