/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine

import android.content.Intent
import android.graphics.Bitmap
import android.view.WindowManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Job
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.ext.toTabSessionState
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.AppIntentState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryItem
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.eq
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
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
            override fun updateTrackingProtection(policy: TrackingProtectionPolicy) {}
            override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {
                notifyObservers { onDesktopModeChange(enable) }
            }
            override fun findAll(text: String) {}
            override fun findNext(forward: Boolean) {}
            override fun clearFindMatches() {}
            override fun exitFullScreenMode() {}
            override fun purgeHistory() {}
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
        val store = BrowserStore()
        store.dispatch(TabListAction.AddTabAction(session.toTabSessionState()))
        engineSession.register(EngineObserver(session, store))

        engineSession.loadUrl("http://mozilla.org")
        engineSession.toggleDesktopMode(true)
        assertEquals("http://mozilla.org", session.url)
        assertEquals(100, session.progress)
        assertEquals(true, session.loading)

        val tab = store.state.findTab(session.id)
        assertNotNull(tab!!)
        assertTrue(tab.content.canGoForward)
        assertTrue(tab.content.canGoBack)
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
            override fun updateTrackingProtection(policy: TrackingProtectionPolicy) {}
            override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {}
            override fun findAll(text: String) {}
            override fun findNext(forward: Boolean) {}
            override fun clearFindMatches() {}
            override fun exitFullScreenMode() {}
            override fun purgeHistory() {}
            override fun loadData(data: String, mimeType: String, encoding: String) {}
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
        engineSession.register(EngineObserver(session, mock()))

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
            override fun updateTrackingProtection(policy: TrackingProtectionPolicy) {}
            override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {}
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
            override fun purgeHistory() {}
        }
        val observer = EngineObserver(session, mock())
        engineSession.register(observer)

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
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)

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

        val observer = EngineObserver(session, mock())
        observer.onTitleChange("Mozilla")

        assertEquals("Mozilla", session.title)

        observer.onLocationChange("https://getpocket.com")

        assertEquals("", session.title)
    }

    @Test
    fun `EngineObserver does not clear title if the URL did not change`() {
        val session = Session("https://www.mozilla.org")
        session.title = "Hello World"

        val observer = EngineObserver(session, mock())
        observer.onTitleChange("Mozilla")

        assertEquals("Mozilla", session.title)

        observer.onLocationChange("https://www.mozilla.org")

        assertEquals("Mozilla", session.title)
    }

    @Test
    fun `EngineObserver does not clear title if the URL changes hash`() {
        val session = Session("https://www.mozilla.org")
        session.title = "Hello World"

        val observer = EngineObserver(session, mock())
        observer.onTitleChange("Mozilla")

        assertEquals("Mozilla", session.title)

        observer.onLocationChange("https://www.mozilla.org/#something")

        assertEquals("Mozilla", session.title)
    }

    @Test
    fun engineObserverClearsBlockedTrackersIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org")
        val observer = EngineObserver(session, mock())

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
        val observer = EngineObserver(session, mock())

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

        val observer = EngineObserver(session, mock())
        observer.onWebAppManifestLoaded(manifest)

        assertEquals(manifest, session.webAppManifest)

        observer.onLocationChange("https://getpocket.com")

        assertNull(session.webAppManifest)
    }

    @Test
    fun engineObserverClearsContentPermissionRequestIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org", id = "sessionId")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)
        val action = ContentAction.ClearPermissionRequests("sessionId")
        doReturn(Job()).`when`(store).dispatch(action)

        runBlockingTest {
            observer.onLocationChange("https://getpocket.com")
            verify(store).dispatch(action)
        }
    }

    @Test
    fun engineObserverDoesNotClearContentPermissionRequestIfSamePageStartsLoading() {
        val session = Session("https://www.mozilla.org")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)
        val action = ContentAction.ClearPermissionRequests("sessionId")
        doReturn(Job()).`when`(store).dispatch(action)

        runBlockingTest {
            observer.onLocationChange("https://www.mozilla.org/hello.html")
            verify(store, never()).dispatch(action)
        }
    }

    @Test
    fun engineObserverDoesNotClearWebAppManifestIfNewPageInStartUrlScope() {
        val session = Session("https://www.mozilla.org")
        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://www.mozilla.org")

        val observer = EngineObserver(session, mock())
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

        val observer = EngineObserver(session, mock())
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
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )
        val observer = EngineObserver(session, store)

        observer.onFindResult(0, 1, false)
        store.waitUntilIdle()
        middleware.assertFirstAction(ContentAction.AddFindResultAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertEquals(FindResultState(0, 1, false), action.findResult)
        }

        observer.onFind("mozilla")
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.ClearFindResultsAction::class) { action ->
            assertEquals(session.id, action.sessionId)
        }
    }

    @Test
    fun engineObserverClearsFindResultIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )
        val observer = EngineObserver(session, store)

        observer.onFindResult(0, 1, false)
        store.waitUntilIdle()
        middleware.assertFirstAction(ContentAction.AddFindResultAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertEquals(FindResultState(0, 1, false), action.findResult)
        }

        observer.onFindResult(1, 2, true)
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.AddFindResultAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertEquals(FindResultState(1, 2, true), action.findResult)
        }

        observer.onLoadingStateChange(true)
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.ClearFindResultsAction::class) { action ->
            assertEquals(session.id, action.sessionId)
        }
    }

    @Test
    fun engineObserverClearsRefreshCanceledIfNewPageStartsLoading() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )
        val observer = EngineObserver(session, store)

        observer.onRepostPromptCancelled()
        store.waitUntilIdle()
        middleware.assertFirstAction(ContentAction.UpdateRefreshCanceledStateAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertTrue(action.refreshCanceled)
        }

        observer.onLoadingStateChange(true)
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.UpdateRefreshCanceledStateAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertFalse(action.refreshCanceled)
        }
    }

    @Test
    fun engineObserverHandlesOnRepostPromptCancelled() {
        val session = Session("")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)

        observer.onRepostPromptCancelled()
        verify(store).dispatch(ContentAction.UpdateRefreshCanceledStateAction(session.id, true))
    }

    @Test
    fun engineObserverHandlesOnBeforeUnloadDenied() {
        val session = Session("")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)

        observer.onBeforeUnloadPromptDenied()
        verify(store).dispatch(ContentAction.UpdateRefreshCanceledStateAction(session.id, true))
    }

    @Test
    fun engineObserverNotifiesFullscreenMode() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )
        val observer = EngineObserver(session, store)

        observer.onFullScreenChange(true)
        store.waitUntilIdle()
        middleware.assertFirstAction(ContentAction.FullScreenChangedAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertTrue(action.fullScreenEnabled)
        }

        observer.onFullScreenChange(false)
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.FullScreenChangedAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertFalse(action.fullScreenEnabled)
        }
    }

    @Test
    fun engineObserverNotifiesDesktopMode() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )
        val observer = EngineObserver(session, store)

        observer.onDesktopModeChange(true)
        store.waitUntilIdle()
        middleware.assertFirstAction(ContentAction.UpdateDesktopModeAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertTrue(action.enabled)
        }

        observer.onDesktopModeChange(false)
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.UpdateDesktopModeAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertFalse(action.enabled)
        }
    }

    @Test
    fun engineObserverNotifiesMetaViewportFitChange() {
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )
        val session = Session("https://www.mozilla.org", id = "test-id")
        val observer = EngineObserver(session, store)

        observer.onMetaViewportFitChanged(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT)
        store.waitUntilIdle()
        middleware.assertFirstAction(ContentAction.ViewportFitChangedAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                action.layoutInDisplayCutoutMode)
        }

        observer.onMetaViewportFitChanged(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES)
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.ViewportFitChangedAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                action.layoutInDisplayCutoutMode)
        }

        observer.onMetaViewportFitChanged(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER)
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.ViewportFitChangedAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER,
                action.layoutInDisplayCutoutMode)
        }

        observer.onMetaViewportFitChanged(123)
        store.waitUntilIdle()
        middleware.assertLastAction(ContentAction.ViewportFitChangedAction::class) { action ->
            assertEquals(session.id, action.sessionId)
            assertEquals(123, action.layoutInDisplayCutoutMode)
        }
    }

    @Test
    fun `Engine observer notified when thumbnail is assigned`() {
        val session = Session("https://www.mozilla.org", id = "test-id")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)
        val emptyBitmap = spy(Bitmap::class.java)
        observer.onThumbnailChange(emptyBitmap)

        verify(store).dispatch(ContentAction.UpdateThumbnailAction(
            "test-id", emptyBitmap
        ))
    }

    @Test
    fun engineObserverNotifiesWebAppManifest() {
        val session = Session("https://www.mozilla.org")
        val observer = EngineObserver(session, mock())
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
        val session = Session("url", id = "id")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)
        val action = ContentAction.UpdatePermissionsRequest(
            session.id,
            permissionRequest
        )
        doReturn(Job()).`when`(store).dispatch(action)

        runBlockingTest {
            observer.onContentPermissionRequest(permissionRequest)
            verify(store).dispatch(action)
        }
    }

    @Test
    fun engineSessionObserverWithAppPermissionRequests() {
        val permissionRequest = mock(PermissionRequest::class.java)
        val session = Session("")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)
        val action = ContentAction.UpdateAppPermissionsRequest(
            session.id,
            permissionRequest
        )

        runBlockingTest {
            observer.onAppPermissionRequest(permissionRequest)
            verify(store).dispatch(action)
        }
    }

    @Test
    fun engineObserverHandlesPromptRequest() {
        val promptRequest = mock(PromptRequest::class.java)
        val session = Session(id = "test-session", initialUrl = "")
        val store: BrowserStore = mock()
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
        val store: BrowserStore = mock()
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
        val store: BrowserStore = mock()
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
    fun `media session state is null by default`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        store.waitUntilIdle()
        assertNull(observedMediaSessionState)
    }

    @Test
    fun `onMediaActivated will update the store`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        val mediaSessionController: MediaSession.Controller = mock()

        observer.onMediaActivated(mediaSessionController)
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNotNull(observedMediaSessionState)
        assertEquals(mediaSessionController, observedMediaSessionState?.controller)
    }

    @Test
    fun `onMediaDeactivated will update the store`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)

        observer.onMediaDeactivated()
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNull(observedMediaSessionState)
    }

    @Test
    fun `onMediaMetadataChanged will update the store`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        val mediaSessionController: MediaSession.Controller = mock()
        val metaData: MediaSession.Metadata = mock()

        observer.onMediaActivated(mediaSessionController)
        store.waitUntilIdle()
        observer.onMediaMetadataChanged(metaData)
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNotNull(observedMediaSessionState)
        assertEquals(mediaSessionController, observedMediaSessionState?.controller)
        assertEquals(metaData, observedMediaSessionState?.metadata)
    }

    @Test
    fun `onMediaPlaybackStateChanged will update the store`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        val mediaSessionController: MediaSession.Controller = mock()
        val playbackState: MediaSession.PlaybackState = mock()

        observer.onMediaActivated(mediaSessionController)
        store.waitUntilIdle()
        observer.onMediaPlaybackStateChanged(playbackState)
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNotNull(observedMediaSessionState)
        assertEquals(mediaSessionController, observedMediaSessionState?.controller)
        assertEquals(playbackState, observedMediaSessionState?.playbackState)
    }

    @Test
    fun `onMediaFeatureChanged will update the store`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        val mediaSessionController: MediaSession.Controller = mock()
        val features: MediaSession.Feature = mock()

        observer.onMediaActivated(mediaSessionController)
        store.waitUntilIdle()
        observer.onMediaFeatureChanged(features)
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNotNull(observedMediaSessionState)
        assertEquals(mediaSessionController, observedMediaSessionState?.controller)
        assertEquals(features, observedMediaSessionState?.features)
    }

    @Test
    fun `onMediaPositionStateChanged will update the store`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        val mediaSessionController: MediaSession.Controller = mock()
        val positionState: MediaSession.PositionState = mock()

        observer.onMediaActivated(mediaSessionController)
        store.waitUntilIdle()
        observer.onMediaPositionStateChanged(positionState)
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNotNull(observedMediaSessionState)
        assertEquals(mediaSessionController, observedMediaSessionState?.controller)
        assertEquals(positionState, observedMediaSessionState?.positionState)
    }

    @Test
    fun `onMediaMuteChanged will update the store`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        val mediaSessionController: MediaSession.Controller = mock()

        observer.onMediaActivated(mediaSessionController)
        store.waitUntilIdle()
        observer.onMediaMuteChanged(true)
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNotNull(observedMediaSessionState)
        assertEquals(mediaSessionController, observedMediaSessionState?.controller)
        assertEquals(true, observedMediaSessionState?.muted)
    }

    @Test
    fun `onMediaFullscreenChanged will update the store`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        val mediaSessionController: MediaSession.Controller = mock()
        val elementMetadata: MediaSession.ElementMetadata = mock()

        observer.onMediaActivated(mediaSessionController)
        store.waitUntilIdle()
        observer.onMediaFullscreenChanged(true, elementMetadata)
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNotNull(observedMediaSessionState)
        assertEquals(mediaSessionController, observedMediaSessionState?.controller)
        assertEquals(true, observedMediaSessionState?.fullscreen)
        assertEquals(elementMetadata, observedMediaSessionState?.elementMetadata)
    }

    @Test
    fun `updates are ignored when media sessoin is deactivated`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val session = Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
        }
        val observer = EngineObserver(session, store)
        val elementMetadata: MediaSession.ElementMetadata = mock()

        observer.onMediaFullscreenChanged(true, elementMetadata)
        store.waitUntilIdle()

        val observedMediaSessionState = store.state.findTab(session.id)?.mediaSessionState
        assertNull(observedMediaSessionState)

        observer.onMediaMuteChanged(true)
        store.waitUntilIdle()
        assertNull(observedMediaSessionState)
    }

    @Test
    fun `onExternalResource will update the store`() {
        val store = BrowserStore()
        val response = mock<Response>()
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
                isPrivate = true,
                contentLength = 100L,
                response = response)

        store.waitUntilIdle()

        val tab = store.state.findTab("test-tab")!!

        assertEquals("mozilla.org/file.txt", tab.content.download?.url)
        assertEquals("file.txt", tab.content.download?.fileName)
        assertEquals("userAgent", tab.content.download?.userAgent)
        assertEquals("text/plain", tab.content.download?.contentType)
        assertEquals(100L, tab.content.download?.contentLength)
        assertEquals(true, tab.content.download?.private)
        assertEquals(response, tab.content.download?.response)
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
        val session = Session("https://www.mozilla.org", id = "test-id")

        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)

        observer.onCrash()

        verify(store).dispatch(CrashAction.SessionCrashedAction(
            "test-id"
        ))
    }

    @Test
    fun `onLocationChange does not clear search terms`() {
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )

        val session = Session("https://www.mozilla.org")

        val observer = EngineObserver(session, store)
        observer.onLocationChange("https://www.mozilla.org/en-US/")

        store.waitUntilIdle()

        middleware.assertNotDispatched(ContentAction.UpdateSearchTermsAction::class)
    }

    @Test
    fun `onLoadRequest clears search terms for requests triggered by web content`() {
        val url = "https://www.mozilla.org"
        val session = Session(url, id = "test-id")

        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )

        val observer = EngineObserver(session, store)
        observer.onLoadRequest(url = url, triggeredByRedirect = false, triggeredByWebContent = true)

        store.waitUntilIdle()

        middleware.assertFirstAction(ContentAction.UpdateSearchTermsAction::class) { action ->
            assertEquals("", action.searchTerms)
            assertEquals("test-id", action.sessionId)
        }
    }

    @Test
    fun `onLoadRequest clears search terms for requests triggered by redirect`() {
        val url = "https://www.mozilla.org"
        val session = Session(url, id = "test-id")

        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )

        val observer = EngineObserver(session, store)
        observer.onLoadRequest(url = url, triggeredByRedirect = true, triggeredByWebContent = false)

        store.waitUntilIdle()

        middleware.assertFirstAction(ContentAction.UpdateSearchTermsAction::class) { action ->
            assertEquals("", action.searchTerms)
            assertEquals("test-id", action.sessionId)
        }
    }

    @Test
    @Suppress("DEPRECATION") // Session observable is deprecated
    fun `onLoadRequest notifies session observers`() {
        val url = "https://www.mozilla.org"
        val sessionObserver: Session.Observer = mock()
        val session = Session(url)
        session.register(sessionObserver)

        val observer = EngineObserver(session, mock())
        observer.onLoadRequest(url = url, triggeredByRedirect = true, triggeredByWebContent = false)
        verify(sessionObserver).onLoadRequest(eq(session), eq(url), eq(true), eq(false))
    }

    @Test
    fun `onLoadRequest does not clear search terms for requests not triggered by user interacting with web content`() {
        val url = "https://www.mozilla.org"
        val session = Session(url, id = "test-id")

        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )

        val observer = EngineObserver(session, store)
        observer.onLoadRequest(url = url, triggeredByRedirect = false, triggeredByWebContent = false)

        store.waitUntilIdle()
        middleware.assertNotDispatched(ContentAction.UpdateSearchTermsAction::class)
    }

    @Test
    fun `onLaunchIntentRequest dispatches UpdateAppIntentAction`() {
        val url = "https://www.mozilla.org"
        val session = Session(url)

        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)
        val intent: Intent = mock()
        observer.onLaunchIntentRequest(url = url, appIntent = intent)

        verify(store).dispatch(ContentAction.UpdateAppIntentAction(session.id, AppIntentState(url, intent)))
    }

    @Test
    fun `onNavigateBack clears search terms when navigating back`() {
        val url = "https://www.mozilla.org"
        val session = Session(url, id = "test-id")

        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(
            middleware = listOf(middleware)
        )

        val observer = EngineObserver(session, store)
        observer.onNavigateBack()
        store.waitUntilIdle()

        middleware.assertFirstAction(ContentAction.UpdateSearchTermsAction::class) { action ->
            assertEquals("", action.searchTerms)
            assertEquals("test-id", action.sessionId)
        }
    }

    @Test
    fun `onHistoryStateChanged dispatches UpdateHistoryStateAction`() {
        val session = Session("")
        val store: BrowserStore = mock()
        val observer = EngineObserver(session, store)

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

    @Test
    fun `equality between tracking protection policies`() {
        val strict = EngineSession.TrackingProtectionPolicy.strict()
        val recommended = EngineSession.TrackingProtectionPolicy.recommended()
        val none = EngineSession.TrackingProtectionPolicy.none()
        val custom = EngineSession.TrackingProtectionPolicy.select(
            trackingCategories = emptyArray(),
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_ONLY_FIRST_PARTY,
            cookiePurging = true,
            strictSocialTrackingProtection = true
        )
        val custom2 = EngineSession.TrackingProtectionPolicy.select(
            trackingCategories = emptyArray(),
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_ONLY_FIRST_PARTY,
            cookiePurging = true,
            strictSocialTrackingProtection = true
        )

        val customNone = EngineSession.TrackingProtectionPolicy.select(
            trackingCategories = none.trackingCategories,
            cookiePolicy = none.cookiePolicy,
            cookiePurging = none.cookiePurging,
            strictSocialTrackingProtection = false
        )

        assertTrue(strict == EngineSession.TrackingProtectionPolicy.strict())
        assertTrue(recommended == EngineSession.TrackingProtectionPolicy.recommended())
        assertTrue(none == EngineSession.TrackingProtectionPolicy.none())
        assertTrue(custom == custom2)

        assertFalse(strict == EngineSession.TrackingProtectionPolicy.strict().forPrivateSessionsOnly())
        assertFalse(recommended == EngineSession.TrackingProtectionPolicy.recommended().forPrivateSessionsOnly())
        assertFalse(custom == custom2.forPrivateSessionsOnly())

        assertFalse(strict == EngineSession.TrackingProtectionPolicy.strict().forRegularSessionsOnly())
        assertFalse(recommended == EngineSession.TrackingProtectionPolicy.recommended().forRegularSessionsOnly())
        assertFalse(custom == custom2.forRegularSessionsOnly())

        assertFalse(none == customNone)
    }
}
