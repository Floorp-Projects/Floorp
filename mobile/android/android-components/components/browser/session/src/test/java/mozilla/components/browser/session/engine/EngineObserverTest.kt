/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine

import android.content.Intent
import android.graphics.Bitmap
import android.view.WindowManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.engine.request.LoadRequestOption
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryItem
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class EngineObserverTest {

    @Test
    fun engineSessionObserver() {
        val session = Session("")
        val engineSession = object : EngineSession() {
            override val settings: Settings
                get() = mock(Settings::class.java)

            override fun goBack() {}
            override fun goForward() {}
            override fun goToHistoryIndex(index: Int) {}
            override fun reload(flags: LoadUrlFlags) {}
            override fun stopLoading() {}
            override fun restoreState(state: EngineSessionState): Boolean { return false }
            override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {}
            override fun disableTrackingProtection() {}
            override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {
                notifyObservers { onDesktopModeChange(enable) }
            }
            override fun findAll(text: String) {}
            override fun findNext(forward: Boolean) {}
            override fun clearFindMatches() {}
            override fun exitFullScreenMode() {}
            override fun saveState(): EngineSessionState = mock()
            override fun recoverFromCrash(): Boolean { return false }

            override fun loadData(data: String, mimeType: String, encoding: String) {
                notifyObservers { onLocationChange(data) }
                notifyObservers { onProgress(100) }
                notifyObservers { onLoadingStateChange(true) }
                notifyObservers { onNavigationStateChange(true, true) }
            }
            override fun loadUrl(
                url: String,
                parent: EngineSession?,
                flags: LoadUrlFlags,
                additionalHeaders: Map<String, String>?
            ) {
                notifyObservers { onLocationChange(url) }
                notifyObservers { onProgress(100) }
                notifyObservers { onLoadingStateChange(true) }
                notifyObservers { onNavigationStateChange(true, true) }
            }
        }
        engineSession.register(EngineObserver(session))

        engineSession.loadUrl("http://mozilla.org")
        engineSession.toggleDesktopMode(true)
        assertEquals("http://mozilla.org", session.url)
        assertEquals("", session.searchTerms)
        assertEquals(100, session.progress)
        assertEquals(true, session.loading)
        assertEquals(true, session.canGoForward)
        assertEquals(true, session.canGoBack)
        assertEquals(true, session.desktopMode)
    }

    @Test
    fun engineSessionObserverWithSecurityChanges() {
        val session = Session("")
        val engineSession = object : EngineSession() {
            override val settings: Settings
                get() = mock(Settings::class.java)

            override fun goBack() {}
            override fun goForward() {}
            override fun goToHistoryIndex(index: Int) {}
            override fun stopLoading() {}
            override fun reload(flags: LoadUrlFlags) {}
            override fun restoreState(state: EngineSessionState): Boolean { return false }
            override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {}
            override fun disableTrackingProtection() {}
            override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {}
            override fun findAll(text: String) {}
            override fun findNext(forward: Boolean) {}
            override fun clearFindMatches() {}
            override fun exitFullScreenMode() {}
            override fun saveState(): EngineSessionState = mock()
            override fun loadData(data: String, mimeType: String, encoding: String) {}
            override fun recoverFromCrash(): Boolean { return false }
            override fun loadUrl(
                url: String,
                parent: EngineSession?,
                flags: LoadUrlFlags,
                additionalHeaders: Map<String, String>?
            ) {
                if (url.startsWith("https://")) {
                    notifyObservers { onSecurityChange(true, "host", "issuer") }
                } else {
                    notifyObservers { onSecurityChange(false) }
                }
            }
        }
        engineSession.register(EngineObserver(session))

        engineSession.loadUrl("http://mozilla.org")
        assertEquals(Session.SecurityInfo(false), session.securityInfo)

        engineSession.loadUrl("https://mozilla.org")
        assertEquals(Session.SecurityInfo(true, "host", "issuer"), session.securityInfo)
    }

    @Test
    fun engineSessionObserverWithTrackingProtection() {
        val session = Session("")
        val engineSession = object : EngineSession() {
            override val settings: Settings
                get() = mock(Settings::class.java)

            override fun goBack() {}
            override fun goForward() {}
            override fun goToHistoryIndex(index: Int) {}
            override fun stopLoading() {}
            override fun reload(flags: LoadUrlFlags) {}
            override fun restoreState(state: EngineSessionState): Boolean { return false }
            override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {
                notifyObservers { onTrackerBlockingEnabledChange(true) }
            }
            override fun disableTrackingProtection() {
                notifyObservers { onTrackerBlockingEnabledChange(false) }
            }

            override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {}
            override fun saveState(): EngineSessionState = mock()
            override fun loadUrl(
                url: String,
                parent: EngineSession?,
                flags: LoadUrlFlags,
                additionalHeaders: Map<String, String>?
            ) {}
            override fun loadData(data: String, mimeType: String, encoding: String) {}
            override fun findAll(text: String) {}
            override fun findNext(forward: Boolean) {}
            override fun clearFindMatches() {}
            override fun exitFullScreenMode() {}
            override fun recoverFromCrash(): Boolean { return false }
        }
        val observer = EngineObserver(session)
        engineSession.register(observer)

        engineSession.enableTrackingProtection()
        assertTrue(session.trackerBlockingEnabled)

        engineSession.disableTrackingProtection()
        assertFalse(session.trackerBlockingEnabled)

        val tracker1 = Tracker("tracker1", emptyList())
        val tracker2 = Tracker("tracker2", emptyList())

        observer.onTrackerBlocked(tracker1)
        assertEquals(listOf(tracker1), session.trackersBlocked)

        observer.onTrackerBlocked(tracker2)
        assertEquals(listOf(tracker1, tracker2), session.trackersBlocked)
    }

    @Test
    fun engineSessionObserverExcludedOnTrackingProtection() {
        val session = Session("")
        val store = mock(BrowserStore::class.java)
        val observer = EngineObserver(session, store)

        whenever(store.dispatch(any())).thenReturn(mock())
        observer.onExcludedOnTrackingProtectionChange(true)

        verify(store).dispatch(
            TrackingProtectionAction.ToggleExclusionListAction(
                session.id,
                true
            )
        )
    }

    @Test
    fun engineObserverClearsWebsiteTitleIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org")
        session.title = "Hello World"

        val observer = EngineObserver(session)
        observer.onTitleChange("Mozilla")

        assertEquals("Mozilla", session.title)

        observer.onLocationChange("https://getpocket.com")

        assertEquals("", session.title)
    }

    @Test
    fun `EngineObserver does not clear title if the URL did not change`() {
        val session = Session("https://www.mozilla.org")
        session.title = "Hello World"

        val observer = EngineObserver(session)
        observer.onTitleChange("Mozilla")

        assertEquals("Mozilla", session.title)

        observer.onLocationChange("https://www.mozilla.org")

        assertEquals("Mozilla", session.title)
    }

    @Test
    fun `EngineObserver does not clear title if the URL changes hash`() {
        val session = Session("https://www.mozilla.org")
        session.title = "Hello World"

        val observer = EngineObserver(session)
        observer.onTitleChange("Mozilla")

        assertEquals("Mozilla", session.title)

        observer.onLocationChange("https://www.mozilla.org/#something")

        assertEquals("Mozilla", session.title)
    }

    @Test
    fun engineObserverClearsBlockedTrackersIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org")
        val observer = EngineObserver(session)

        val tracker1 = Tracker("tracker1")
        val tracker2 = Tracker("tracker2")
        observer.onTrackerBlocked(tracker1)
        observer.onTrackerBlocked(tracker2)
        assertEquals(listOf(tracker1, tracker2), session.trackersBlocked)

        observer.onLoadingStateChange(true)
        assertEquals(emptyList<String>(), session.trackersBlocked)
    }

    @Test
    fun engineObserverClearsLoadedTrackersIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org")
        val observer = EngineObserver(session)

        val tracker1 = Tracker("tracker1")
        val tracker2 = Tracker("tracker2")
        observer.onTrackerLoaded(tracker1)
        observer.onTrackerLoaded(tracker2)
        assertEquals(listOf(tracker1, tracker2), session.trackersLoaded)

        observer.onLoadingStateChange(true)
        assertEquals(emptyList<String>(), session.trackersLoaded)
    }

    @Test
    fun engineObserverClearsWebAppManifestIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org")
        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")

        val observer = EngineObserver(session)
        observer.onWebAppManifestLoaded(manifest)

        assertEquals(manifest, session.webAppManifest)

        observer.onLocationChange("https://getpocket.com")

        assertNull(session.webAppManifest)
    }

    @Test
    fun engineObserverClearsContentPermissionRequestIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org")
        val permissionRequest: PermissionRequest = mock()
        val observer = EngineObserver(session)

        observer.onContentPermissionRequest(permissionRequest)

        observer.onLocationChange("https://getpocket.com")

        assertTrue(session.contentPermissionRequest.isConsumed())
        verify(permissionRequest).reject()
    }

    @Test
    fun engineObserverDoesNotClearContentPermissionRequestIfSamePageStartsLoading() {
        val session = Session("https://www.mozilla.org")
        val permissionRequest: PermissionRequest = mock()
        val observer = EngineObserver(session)

        observer.onContentPermissionRequest(permissionRequest)

        observer.onLocationChange("https://www.mozilla.org/hello.html")

        assertFalse(session.contentPermissionRequest.isConsumed())
        verify(permissionRequest, never()).reject()
    }

    @Test
    fun engineObserverDoesNotClearWebAppManifestIfNewPageInStartUrlScope() {
        val session = Session("https://www.mozilla.org")
        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://www.mozilla.org")

        val observer = EngineObserver(session)
        observer.onWebAppManifestLoaded(manifest)

        assertEquals(manifest, session.webAppManifest)

        observer.onLocationChange("https://www.mozilla.org/hello.html")

        assertEquals(manifest, session.webAppManifest)
    }

    @Test
    fun engineObserverDoesNotClearWebAppManifestIfNewPageInScope() {
        val session = Session("https://www.mozilla.org/hello/page1.html")
        val manifest = WebAppManifest(
            name = "Mozilla",
            startUrl = "https://www.mozilla.org",
            scope = "https://www.mozilla.org/hello/"
        )

        val observer = EngineObserver(session)
        observer.onWebAppManifestLoaded(manifest)

        assertEquals(manifest, session.webAppManifest)

        observer.onLocationChange("https://www.mozilla.org/hello/page2.html")
        assertEquals(manifest, session.webAppManifest)

        observer.onLocationChange("https://www.mozilla.org/hello.html")
        assertNull(session.webAppManifest)
    }

    @Test
    fun engineObserverPassingHitResult() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)
        val hitResult = HitResult.UNKNOWN("data://foobar")

        observer.onLongPress(hitResult)

        verify(store).dispatch(
            ContentAction.UpdateHitResultAction("test-id", hitResult)
        )
    }

    @Test
    fun engineObserverClearsFindResults() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)

        observer.onFindResult(0, 1, false)

        verify(store).dispatch(ContentAction.AddFindResultAction(
            "test-id", FindResultState(0, 1, false)
        ))
        reset(store)

        observer.onFind("mozilla")

        verify(store).dispatch(
            ContentAction.ClearFindResultsAction("test-id")
        )
    }

    @Test
    fun engineObserverClearsFindResultIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)

        observer.onFindResult(0, 1, false)

        verify(store).dispatch(ContentAction.AddFindResultAction(
            "test-id", FindResultState(0, 1, false)
        ))
        reset(store)

        observer.onFindResult(1, 2, true)

        verify(store).dispatch(ContentAction.AddFindResultAction(
            "test-id", FindResultState(1, 2, true)
        ))
        reset(store)

        observer.onLoadingStateChange(true)

        verify(store).dispatch(ContentAction.ClearFindResultsAction("test-id"))
    }

    @Test
    fun engineObserverNotifiesFullscreenMode() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)

        observer.onFullScreenChange(true)

        verify(store).dispatch(ContentAction.FullScreenChangedAction(
            "test-id", true
        ))
        reset(store)

        observer.onFullScreenChange(false)

        verify(store).dispatch(ContentAction.FullScreenChangedAction(
            "test-id", false
        ))
    }

    @Test
    fun engineObserverNotifiesMetaViewportFitChange() {
        val store: BrowserStore = mock()
        val session = Session("https://www.mozilla.org", id = "test-id")
        val observer = EngineObserver(session, store)

        observer.onMetaViewportFitChanged(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT)
        verify(store).dispatch(ContentAction.ViewportFitChangedAction(
            "test-id", WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT
        ))
        reset(store)

        observer.onMetaViewportFitChanged(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES)
        verify(store).dispatch(ContentAction.ViewportFitChangedAction(
            "test-id", WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        ))
        reset(store)

        observer.onMetaViewportFitChanged(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER)
        verify(store).dispatch(ContentAction.ViewportFitChangedAction(
            "test-id", WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER
        ))
        reset(store)

        observer.onMetaViewportFitChanged(123)
        verify(store).dispatch(ContentAction.ViewportFitChangedAction(
            "test-id", 123
        ))
        reset(store)
    }

    @Test
    fun `Engine observer notified when thumbnail is assigned`() {
        val session = Session("https://www.mozilla.org")
        val observer = EngineObserver(session)
        val emptyBitmap = spy(Bitmap::class.java)
        observer.onThumbnailChange(emptyBitmap)
        assertEquals(emptyBitmap, session.thumbnail)
    }

    @Test
    fun engineObserverNotifiesWebAppManifest() {
        val session = Session("https://www.mozilla.org")
        val observer = EngineObserver(session)
        val manifest = WebAppManifest(
            name = "Minimal",
            startUrl = "/"
        )

        observer.onWebAppManifestLoaded(manifest)
        assertEquals(manifest, session.webAppManifest)
    }

    @Test
    fun engineSessionObserverWithContentPermissionRequests() {
        val permissionRequest = mock(PermissionRequest::class.java)
        val session = Session("")
        val observer = EngineObserver(session)

        assertTrue(session.contentPermissionRequest.isConsumed())
        observer.onContentPermissionRequest(permissionRequest)
        assertFalse(session.contentPermissionRequest.isConsumed())

        observer.onCancelContentPermissionRequest(permissionRequest)
        assertTrue(session.contentPermissionRequest.isConsumed())
    }

    @Test
    fun engineSessionObserverWithAppPermissionRequests() {
        val permissionRequest = mock(PermissionRequest::class.java)
        val session = Session("")
        val observer = EngineObserver(session)

        assertTrue(session.appPermissionRequest.isConsumed())
        observer.onAppPermissionRequest(permissionRequest)
        assertFalse(session.appPermissionRequest.isConsumed())
    }

    @Test
    fun engineObserverConsumesContentPermissionRequestIfNewPageStartsLoading() {
        val permissionRequest = mock(PermissionRequest::class.java)
        val session = Session("https://www.mozilla.org")
        session.contentPermissionRequest = Consumable.from(permissionRequest)

        val observer = EngineObserver(session)
        observer.onLocationChange("https://getpocket.com")

        verify(permissionRequest).reject()
        assertTrue(session.contentPermissionRequest.isConsumed())
    }

    @Test
    fun engineObserverHandlesPromptRequest() {
        val promptRequest = mock(PromptRequest::class.java)
        val session = Session(id = "test-session", initialUrl = "")
        val store = mock(BrowserStore::class.java)
        val observer = EngineObserver(session, store)

        observer.onPromptRequest(promptRequest)
        verify(store).dispatch(ContentAction.UpdatePromptRequestAction(
            session.id,
            promptRequest
        ))
    }

    @Test
    fun engineObserverHandlesWindowRequest() {
        val windowRequest = mock(WindowRequest::class.java)
        val session = Session("")
        val store = mock(BrowserStore::class.java)
        whenever(store.state).thenReturn(mock())
        val observer = EngineObserver(session, store)

        observer.onWindowRequest(windowRequest)
        verify(store).dispatch(ContentAction.UpdateWindowRequestAction(
            session.id,
            windowRequest
        ))
    }

    @Test
    fun engineObserverHandlesFirstContentfulPaint() {
        val session = Session("")
        val store = mock(BrowserStore::class.java)
        whenever(store.state).thenReturn(mock())
        val observer = EngineObserver(session, store)

        observer.onFirstContentfulPaint()
        verify(store).dispatch(ContentAction.UpdateFirstContentfulPaintStateAction(
            session.id,
            true
        ))
    }

    @Test
    fun engineObserverHandlesPaintStatusReset() {
        val session = Session("")
        val store = mock(BrowserStore::class.java)
        whenever(store.state).thenReturn(mock())
        val observer = EngineObserver(session, store)

        observer.onPaintStatusReset()
        verify(store).dispatch(ContentAction.UpdateFirstContentfulPaintStateAction(
            session.id,
            false
        ))
    }

    @Test
    fun `onMediaAdded will subscribe to media and add it to store`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        assertEquals(0, store.state.media.elements.size)

        val media1: Media = spy(object : Media() {
            override val controller: Controller = mock()
            override val metadata: Metadata = mock()
            override val volume: Volume = mock()
            override val fullscreen: Boolean = false
        })
        observer.onMediaAdded(media1)

        store.waitUntilIdle()

        verify(media1).register(any())
        assertEquals(1, store.state.media.elements["test-tab"]?.size)

        val media2: Media = spy(object : Media() {
            override val controller: Controller = mock()
            override val metadata: Metadata = mock()
            override val volume: Volume = mock()
            override val fullscreen: Boolean = false
        })
        observer.onMediaAdded(media2)

        store.waitUntilIdle()

        verify(media2).register(any())
        assertEquals(2, store.state.media.elements["test-tab"]?.size)

        val media3: Media = spy(object : Media() {
            override val controller: Controller = mock()
            override val metadata: Metadata = mock()
            override val volume: Volume = mock()
            override val fullscreen: Boolean = false
        })
        observer.onMediaAdded(media3)

        store.waitUntilIdle()

        verify(media3).register(any())
        assertEquals(3, store.state.media.elements["test-tab"]?.size)
    }

    @Test
    fun `onMediaRemoved will unsubscribe and remove it from store`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)

        val media1: Media = spy(object : Media() {
            override val controller: Controller = mock()
            override val metadata: Metadata = mock()
            override val volume: Volume = mock()
            override val fullscreen: Boolean = false
        })
        observer.onMediaAdded(media1)

        val media2: Media = spy(object : Media() {
            override val controller: Controller = mock()
            override val metadata: Metadata = mock()
            override val volume: Volume = mock()
            override val fullscreen: Boolean = false
        })
        observer.onMediaAdded(media2)

        store.waitUntilIdle()

        assertEquals(2, store.state.media.elements["test-tab"]?.size)
        verify(media1, never()).unregister(any())
        verify(media2, never()).unregister(any())

        observer.onMediaRemoved(media1)
        store.waitUntilIdle()

        assertEquals(1, store.state.media.elements["test-tab"]?.size)
        verify(media1).unregister(any())
        verify(media2, never()).unregister(any())

        observer.onMediaRemoved(media2)
        store.waitUntilIdle()

        assertNull(store.state.media.elements["test-tab"])
        verify(media2).unregister(any())
    }

    @Test
    fun `onExternalResource will update the store`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }

        val observer = EngineObserver(session, store)

        observer.onExternalResource(
                url = "mozilla.org/file.txt",
                fileName = "file.txt",
                userAgent = "userAgent",
                contentType = "text/plain",
                contentLength = 100L)

        store.waitUntilIdle()

        val tab = store.state.findTab("test-tab")!!

        assertEquals("mozilla.org/file.txt", tab.content.download?.url)
        assertEquals("file.txt", tab.content.download?.fileName)
        assertEquals("userAgent", tab.content.download?.userAgent)
        assertEquals("text/plain", tab.content.download?.contentType)
        assertEquals(100L, tab.content.download?.contentLength)
    }

    @Test
    fun `onExternalResource with negative contentLength`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)

        observer.onExternalResource(url = "mozilla.org/file.txt", contentLength = -1)

        store.waitUntilIdle()

        val tab = store.state.findTab("test-tab")!!

        assertNull(tab.content.download?.contentLength)
    }

    @Test
    fun `onCrashStateChanged will update session and notify observer`() {
        val session = Session("https://www.mozilla.org")
        assertFalse(session.crashed)

        val observer = EngineObserver(session)

        observer.onCrash()
        assertTrue(session.crashed)

        session.crashed = false

        observer.onCrash()
        assertTrue(session.crashed)
    }

    @Test
    fun `onLocationChange does not clear search terms`() {
        val session = Session("https://www.mozilla.org")
        session.searchTerms = "Mozilla Foundation"

        val observer = EngineObserver(session)
        observer.onLocationChange("https://www.mozilla.org/en-US/")

        assertEquals("Mozilla Foundation", session.searchTerms)
    }

    @Test
    fun `onLoadRequest clears search terms for requests triggered by web content`() {
        val url = "https://www.mozilla.org"
        val session = Session(url)
        session.searchTerms = "Mozilla Foundation"

        val observer = EngineObserver(session)
        observer.onLoadRequest(url = url, triggeredByRedirect = false, triggeredByWebContent = true)

        assertEquals("", session.searchTerms)
        val triggeredByRedirect = session.loadRequestMetadata.isSet(LoadRequestOption.REDIRECT)
        val triggeredByWebContent = session.loadRequestMetadata.isSet(LoadRequestOption.WEB_CONTENT)

        assertFalse(triggeredByRedirect)
        assertTrue(triggeredByWebContent)
    }

    @Test
    fun `onLoadRequest clears search terms for requests triggered by redirect`() {
        val url = "https://www.mozilla.org"
        val session = Session(url)
        session.searchTerms = "Mozilla Foundation"

        val observer = EngineObserver(session)
        observer.onLoadRequest(url = url, triggeredByRedirect = true, triggeredByWebContent = false)

        assertEquals("", session.searchTerms)
        val triggeredByRedirect = session.loadRequestMetadata.isSet(LoadRequestOption.REDIRECT)
        val triggeredByWebContent = session.loadRequestMetadata.isSet(LoadRequestOption.WEB_CONTENT)

        assertTrue(triggeredByRedirect)
        assertFalse(triggeredByWebContent)
    }

    @Test
    fun `onLoadRequest does not clear search terms for requests not triggered by user interacting with web content`() {
        val url = "https://www.mozilla.org"
        val session = Session(url)
        session.searchTerms = "Mozilla Foundation"

        val observer = EngineObserver(session)
        observer.onLoadRequest(url = url, triggeredByRedirect = false, triggeredByWebContent = false)

        assertEquals("Mozilla Foundation", session.searchTerms)

        val triggeredByRedirect = session.loadRequestMetadata.isSet(LoadRequestOption.REDIRECT)
        val triggeredByWebContent = session.loadRequestMetadata.isSet(LoadRequestOption.WEB_CONTENT)

        assertFalse(triggeredByRedirect)
        assertFalse(triggeredByWebContent)
    }

    @Test
    fun `onLaunchIntentRequest is set to launchIntentMetadata`() {
        val url = "https://www.mozilla.org"
        val session = Session(url)

        val observer = EngineObserver(session)
        val intent: Intent = mock()
        observer.onLaunchIntentRequest(url = url, appIntent = intent)

        val appUrl = session.launchIntentMetadata.url
        val appIntent = session.launchIntentMetadata.appIntent

        assertEquals(url, appUrl)
        assertEquals(intent, appIntent)
    }

    @Test
    fun `onNavigateBack clears search terms when navigating back`() {
        val url = "https://www.mozilla.org"
        val session = Session(url)
        session.searchTerms = "Mozilla Foundation"
        session.canGoBack = true

        val observer = EngineObserver(session)
        observer.onNavigateBack()

        assertEquals("", session.searchTerms)
    }

    @Test
    fun `onHistoryStateChanged dispatches UpdateHistoryStateAction`() {
        val session = Session("")
        val store = mock(BrowserStore::class.java)
        val observer = EngineObserver(session, store)
        whenever(store.dispatch(any())).thenReturn(mock())

        observer.onHistoryStateChanged(emptyList(), 0)
        verify(store).dispatch(
            ContentAction.UpdateHistoryStateAction(
                session.id,
                emptyList(),
                currentIndex = 0
            )
        )

        observer.onHistoryStateChanged(listOf(
            HistoryItem("Firefox", "https://firefox.com"),
            HistoryItem("Mozilla", "http://mozilla.org")
        ), 1)
        verify(store).dispatch(
            ContentAction.UpdateHistoryStateAction(
                session.id,
                listOf(
                    HistoryItem("Firefox", "https://firefox.com"),
                    HistoryItem("Mozilla", "http://mozilla.org")
                ),
                currentIndex = 1
            )
        )
    }
}
