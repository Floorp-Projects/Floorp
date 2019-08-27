/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.graphics.Bitmap
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.verifyZeroInteractions
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy

class EngineSessionTest {
    private val unknownHitResult = HitResult.UNKNOWN("file://foobar")

    @Test
    fun `registered observers will be notified`() {
        val session = spy(DummyEngineSession())

        val observer = mock(EngineSession.Observer::class.java)
        val emptyBitmap = spy(Bitmap::class.java)
        val permissionRequest = mock(PermissionRequest::class.java)
        val windowRequest = mock(WindowRequest::class.java)
        session.register(observer)

        val mediaAdded: Media = mock()
        val mediaRemoved: Media = mock()
        val tracker = Tracker("tracker")

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked(tracker) }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(true) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }
        session.notifyInternalObservers { onFullScreenChange(true) }
        session.notifyInternalObservers { onThumbnailChange(emptyBitmap) }
        session.notifyInternalObservers { onContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onCancelContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onAppPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onOpenWindowRequest(windowRequest) }
        session.notifyInternalObservers { onCloseWindowRequest(windowRequest) }
        session.notifyInternalObservers { onMediaAdded(mediaAdded) }
        session.notifyInternalObservers { onMediaRemoved(mediaRemoved) }
        session.notifyInternalObservers { onCrash() }
        session.notifyInternalObservers { onLoadRequest("https://www.mozilla.org", true, true) }
        session.notifyInternalObservers { onProcessKilled() }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onLocationChange("https://www.firefox.com")
        verify(observer).onProgress(25)
        verify(observer).onProgress(100)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer).onTrackerBlockingEnabledChange(true)
        verify(observer).onTrackerBlocked(tracker)
        verify(observer).onLongPress(unknownHitResult)
        verify(observer).onDesktopModeChange(true)
        verify(observer).onFind("search")
        verify(observer).onFindResult(0, 1, true)
        verify(observer).onFullScreenChange(true)
        verify(observer).onThumbnailChange(emptyBitmap)
        verify(observer).onAppPermissionRequest(permissionRequest)
        verify(observer).onContentPermissionRequest(permissionRequest)
        verify(observer).onCancelContentPermissionRequest(permissionRequest)
        verify(observer).onOpenWindowRequest(windowRequest)
        verify(observer).onCloseWindowRequest(windowRequest)
        verify(observer).onMediaAdded(mediaAdded)
        verify(observer).onMediaRemoved(mediaRemoved)
        verify(observer).onCrash()
        verify(observer).onLoadRequest("https://www.mozilla.org", true, true)
        verify(observer).onProcessKilled()
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer will not be notified after calling unregister`() {
        val session = spy(DummyEngineSession())
        val observer = mock(EngineSession.Observer::class.java)
        val otherHitResult = HitResult.UNKNOWN("file://foobaz")
        val permissionRequest = mock(PermissionRequest::class.java)
        val otherPermissionRequest = mock(PermissionRequest::class.java)
        val windowRequest = mock(WindowRequest::class.java)
        val otherWindowRequest = mock(WindowRequest::class.java)
        val emptyBitmap = spy(Bitmap::class.java)
        val tracker = Tracker("tracker")

        session.register(observer)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked(tracker) }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(true) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }
        session.notifyInternalObservers { onFullScreenChange(true) }
        session.notifyInternalObservers { onThumbnailChange(emptyBitmap) }
        session.notifyInternalObservers { onContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onCancelContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onAppPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onOpenWindowRequest(windowRequest) }
        session.notifyInternalObservers { onCloseWindowRequest(windowRequest) }
        session.notifyInternalObservers { onCrash() }
        session.notifyInternalObservers { onLoadRequest("https://www.mozilla.org", true, true) }
        session.unregister(observer)

        val mediaAdded: Media = mock()
        val mediaRemoved: Media = mock()

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }
        session.notifyInternalObservers { onSecurityChange(false, "", "") }
        session.notifyInternalObservers { onTrackerBlocked(tracker) }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(false) }
        session.notifyInternalObservers { onLongPress(otherHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(false) }
        session.notifyInternalObservers { onFind("search2") }
        session.notifyInternalObservers { onFindResult(0, 1, false) }
        session.notifyInternalObservers { onFullScreenChange(false) }
        session.notifyInternalObservers { onThumbnailChange(null) }
        session.notifyInternalObservers { onContentPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onCancelContentPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onAppPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onOpenWindowRequest(otherWindowRequest) }
        session.notifyInternalObservers { onCloseWindowRequest(otherWindowRequest) }
        session.notifyInternalObservers { onMediaAdded(mediaAdded) }
        session.notifyInternalObservers { onMediaRemoved(mediaRemoved) }
        session.notifyInternalObservers { onCrash() }
        session.notifyInternalObservers { onLoadRequest("https://www.mozilla.org", false, true) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer).onTrackerBlockingEnabledChange(true)
        verify(observer).onTrackerBlocked(tracker)
        verify(observer).onLongPress(unknownHitResult)
        verify(observer).onDesktopModeChange(true)
        verify(observer).onFind("search")
        verify(observer).onFindResult(0, 1, true)
        verify(observer).onFullScreenChange(true)
        verify(observer).onThumbnailChange(emptyBitmap)
        verify(observer).onAppPermissionRequest(permissionRequest)
        verify(observer).onContentPermissionRequest(permissionRequest)
        verify(observer).onCancelContentPermissionRequest(permissionRequest)
        verify(observer).onOpenWindowRequest(windowRequest)
        verify(observer).onCloseWindowRequest(windowRequest)
        verify(observer).onCrash()
        verify(observer).onLoadRequest("https://www.mozilla.org", true, true)
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer, never()).onLoadingStateChange(false)
        verify(observer, never()).onSecurityChange(false, "", "")
        verify(observer, never()).onTrackerBlockingEnabledChange(false)
        verify(observer, never()).onTrackerBlocked(Tracker("Tracker"))
        verify(observer, never()).onLongPress(otherHitResult)
        verify(observer, never()).onDesktopModeChange(false)
        verify(observer, never()).onFind("search2")
        verify(observer, never()).onFindResult(0, 1, false)
        verify(observer, never()).onFullScreenChange(false)
        verify(observer, never()).onThumbnailChange(null)
        verify(observer, never()).onAppPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onContentPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onCancelContentPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onOpenWindowRequest(otherWindowRequest)
        verify(observer, never()).onCloseWindowRequest(otherWindowRequest)
        verify(observer, never()).onMediaAdded(mediaAdded)
        verify(observer, never()).onMediaRemoved(mediaRemoved)
        verify(observer, never()).onLoadRequest("https://www.mozilla.org", false, true)
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observers will not be notified after calling unregisterObservers`() {
        val session = spy(DummyEngineSession())
        val observer = mock(EngineSession.Observer::class.java)
        val otherObserver = mock(EngineSession.Observer::class.java)
        val permissionRequest = mock(PermissionRequest::class.java)
        val otherPermissionRequest = mock(PermissionRequest::class.java)
        val windowRequest = mock(WindowRequest::class.java)
        val otherWindowRequest = mock(WindowRequest::class.java)
        val otherHitResult = HitResult.UNKNOWN("file://foobaz")
        val emptyBitmap = spy(Bitmap::class.java)
        val tracker = Tracker("tracker")

        session.register(observer)
        session.register(otherObserver)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked(tracker) }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(true) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }
        session.notifyInternalObservers { onFullScreenChange(true) }
        session.notifyInternalObservers { onThumbnailChange(emptyBitmap) }
        session.notifyInternalObservers { onContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onCancelContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onAppPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onOpenWindowRequest(windowRequest) }
        session.notifyInternalObservers { onCloseWindowRequest(windowRequest) }

        session.unregisterObservers()

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }
        session.notifyInternalObservers { onSecurityChange(false, "", "") }
        session.notifyInternalObservers { onTrackerBlocked(tracker) }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(false) }
        session.notifyInternalObservers { onLongPress(otherHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(false) }
        session.notifyInternalObservers { onFind("search2") }
        session.notifyInternalObservers { onFindResult(0, 1, false) }
        session.notifyInternalObservers { onFullScreenChange(false) }
        session.notifyInternalObservers { onThumbnailChange(null) }
        session.notifyInternalObservers { onContentPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onCancelContentPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onAppPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onOpenWindowRequest(otherWindowRequest) }
        session.notifyInternalObservers { onCloseWindowRequest(otherWindowRequest) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer).onTrackerBlockingEnabledChange(true)
        verify(observer).onTrackerBlocked(tracker)
        verify(observer).onLongPress(unknownHitResult)
        verify(observer).onDesktopModeChange(true)
        verify(observer).onFind("search")
        verify(observer).onFindResult(0, 1, true)
        verify(observer).onFullScreenChange(true)
        verify(observer).onThumbnailChange(emptyBitmap)
        verify(observer).onAppPermissionRequest(permissionRequest)
        verify(observer).onContentPermissionRequest(permissionRequest)
        verify(observer).onCancelContentPermissionRequest(permissionRequest)
        verify(observer).onOpenWindowRequest(windowRequest)
        verify(observer).onCloseWindowRequest(windowRequest)
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer, never()).onLoadingStateChange(false)
        verify(observer, never()).onSecurityChange(false, "", "")
        verify(observer, never()).onTrackerBlockingEnabledChange(false)
        verify(observer, never()).onTrackerBlocked(Tracker("Tracker"))
        verify(observer, never()).onLongPress(otherHitResult)
        verify(observer, never()).onDesktopModeChange(false)
        verify(observer, never()).onFind("search2")
        verify(observer, never()).onFindResult(0, 1, false)
        verify(observer, never()).onFullScreenChange(false)
        verify(observer, never()).onThumbnailChange(null)
        verify(observer, never()).onAppPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onContentPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onCancelContentPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onOpenWindowRequest(otherWindowRequest)
        verify(observer, never()).onCloseWindowRequest(otherWindowRequest)
        verify(otherObserver, never()).onLocationChange("https://www.firefox.com")
        verify(otherObserver, never()).onProgress(100)
        verify(otherObserver, never()).onLoadingStateChange(false)
        verify(otherObserver, never()).onSecurityChange(false, "", "")
        verify(otherObserver, never()).onTrackerBlockingEnabledChange(false)
        verify(otherObserver, never()).onTrackerBlocked(Tracker("Tracker"))
        verify(otherObserver, never()).onLongPress(otherHitResult)
        verify(otherObserver, never()).onDesktopModeChange(false)
        verify(otherObserver, never()).onFind("search2")
        verify(otherObserver, never()).onFindResult(0, 1, false)
        verify(otherObserver, never()).onFullScreenChange(false)
        verify(otherObserver, never()).onThumbnailChange(null)
        verify(otherObserver, never()).onAppPermissionRequest(otherPermissionRequest)
        verify(otherObserver, never()).onContentPermissionRequest(otherPermissionRequest)
        verify(otherObserver, never()).onCancelContentPermissionRequest(otherPermissionRequest)
        verify(otherObserver, never()).onOpenWindowRequest(otherWindowRequest)
        verify(otherObserver, never()).onCloseWindowRequest(otherWindowRequest)
    }

    @Test
    fun `observer will not be notified after session is closed`() {
        val session = spy(DummyEngineSession())
        val observer = mock(EngineSession.Observer::class.java)
        val otherHitResult = HitResult.UNKNOWN("file://foobaz")
        val permissionRequest = mock(PermissionRequest::class.java)
        val otherPermissionRequest = mock(PermissionRequest::class.java)
        val windowRequest = mock(WindowRequest::class.java)
        val otherWindowRequest = mock(WindowRequest::class.java)
        val emptyBitmap = spy(Bitmap::class.java)
        val tracker = Tracker("tracker")

        session.register(observer)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked(tracker) }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(true) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }
        session.notifyInternalObservers { onFullScreenChange(true) }
        session.notifyInternalObservers { onThumbnailChange(emptyBitmap) }
        session.notifyInternalObservers { onContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onCancelContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onAppPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onOpenWindowRequest(windowRequest) }
        session.notifyInternalObservers { onCloseWindowRequest(windowRequest) }

        session.close()

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }
        session.notifyInternalObservers { onSecurityChange(false, "", "") }
        session.notifyInternalObservers { onTrackerBlocked(tracker) }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(false) }
        session.notifyInternalObservers { onLongPress(otherHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(false) }
        session.notifyInternalObservers { onFind("search2") }
        session.notifyInternalObservers { onFindResult(0, 1, false) }
        session.notifyInternalObservers { onFullScreenChange(false) }
        session.notifyInternalObservers { onThumbnailChange(null) }
        session.notifyInternalObservers { onContentPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onCancelContentPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onAppPermissionRequest(otherPermissionRequest) }
        session.notifyInternalObservers { onOpenWindowRequest(otherWindowRequest) }
        session.notifyInternalObservers { onCloseWindowRequest(otherWindowRequest) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer).onTrackerBlockingEnabledChange(true)
        verify(observer).onTrackerBlocked(tracker)
        verify(observer).onLongPress(unknownHitResult)
        verify(observer).onDesktopModeChange(true)
        verify(observer).onFind("search")
        verify(observer).onFindResult(0, 1, true)
        verify(observer).onFullScreenChange(true)
        verify(observer).onThumbnailChange(emptyBitmap)
        verify(observer).onAppPermissionRequest(permissionRequest)
        verify(observer).onContentPermissionRequest(permissionRequest)
        verify(observer).onCancelContentPermissionRequest(permissionRequest)
        verify(observer).onOpenWindowRequest(windowRequest)
        verify(observer).onCloseWindowRequest(windowRequest)
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer, never()).onLoadingStateChange(false)
        verify(observer, never()).onSecurityChange(false, "", "")
        verify(observer, never()).onTrackerBlockingEnabledChange(false)
        verify(observer, never()).onTrackerBlocked(Tracker("Tracker"))
        verify(observer, never()).onLongPress(otherHitResult)
        verify(observer, never()).onDesktopModeChange(false)
        verify(observer, never()).onFind("search2")
        verify(observer, never()).onFindResult(0, 1, false)
        verify(observer, never()).onFullScreenChange(false)
        verify(observer, never()).onThumbnailChange(null)
        verify(observer, never()).onAppPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onContentPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onCancelContentPermissionRequest(otherPermissionRequest)
        verify(observer, never()).onOpenWindowRequest(otherWindowRequest)
        verify(observer, never()).onCloseWindowRequest(otherWindowRequest)
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `registered observers are instance specific`() {
        val session = spy(DummyEngineSession())
        val otherSession = spy(DummyEngineSession())
        val permissionRequest = mock(PermissionRequest::class.java)
        val windowRequest = mock(WindowRequest::class.java)
        val emptyBitmap = spy(Bitmap::class.java)
        val observer = mock(EngineSession.Observer::class.java)
        val tracker = Tracker("tracker")
        session.register(observer)

        otherSession.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        otherSession.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        otherSession.notifyInternalObservers { onProgress(25) }
        otherSession.notifyInternalObservers { onLoadingStateChange(true) }
        otherSession.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        otherSession.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        otherSession.notifyInternalObservers { onTrackerBlocked(tracker) }
        otherSession.notifyInternalObservers { onLongPress(unknownHitResult) }
        otherSession.notifyInternalObservers { onDesktopModeChange(true) }
        otherSession.notifyInternalObservers { onFind("search") }
        otherSession.notifyInternalObservers { onFindResult(0, 1, true) }
        otherSession.notifyInternalObservers { onFullScreenChange(true) }
        otherSession.notifyInternalObservers { onThumbnailChange(emptyBitmap) }
        otherSession.notifyInternalObservers { onContentPermissionRequest(permissionRequest) }
        otherSession.notifyInternalObservers { onCancelContentPermissionRequest(permissionRequest) }
        otherSession.notifyInternalObservers { onAppPermissionRequest(permissionRequest) }
        otherSession.notifyInternalObservers { onOpenWindowRequest(windowRequest) }
        otherSession.notifyInternalObservers { onCloseWindowRequest(windowRequest) }
        verify(observer, never()).onLocationChange("https://www.mozilla.org")
        verify(observer, never()).onProgress(25)
        verify(observer, never()).onLoadingStateChange(true)
        verify(observer, never()).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer, never()).onTrackerBlockingEnabledChange(true)
        verify(observer, never()).onTrackerBlocked(tracker)
        verify(observer, never()).onLongPress(unknownHitResult)
        verify(observer, never()).onDesktopModeChange(true)
        verify(observer, never()).onFind("search")
        verify(observer, never()).onFindResult(0, 1, true)
        verify(observer, never()).onFullScreenChange(true)
        verify(observer, never()).onThumbnailChange(emptyBitmap)
        verify(observer, never()).onAppPermissionRequest(permissionRequest)
        verify(observer, never()).onContentPermissionRequest(permissionRequest)
        verify(observer, never()).onCancelContentPermissionRequest(permissionRequest)
        verify(observer, never()).onOpenWindowRequest(windowRequest)
        verify(observer, never()).onCloseWindowRequest(windowRequest)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked(tracker) }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(false) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }
        session.notifyInternalObservers { onFullScreenChange(true) }
        session.notifyInternalObservers { onThumbnailChange(emptyBitmap) }
        session.notifyInternalObservers { onContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onCancelContentPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onAppPermissionRequest(permissionRequest) }
        session.notifyInternalObservers { onOpenWindowRequest(windowRequest) }
        session.notifyInternalObservers { onCloseWindowRequest(windowRequest) }
        verify(observer, times(1)).onLocationChange("https://www.mozilla.org")
        verify(observer, times(1)).onProgress(25)
        verify(observer, times(1)).onLoadingStateChange(true)
        verify(observer, times(1)).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer, times(1)).onTrackerBlockingEnabledChange(true)
        verify(observer, times(1)).onTrackerBlocked(tracker)
        verify(observer, times(1)).onLongPress(unknownHitResult)
        verify(observer, times(1)).onDesktopModeChange(false)
        verify(observer, times(1)).onFind("search")
        verify(observer, times(1)).onFindResult(0, 1, true)
        verify(observer, times(1)).onFullScreenChange(true)
        verify(observer, times(1)).onThumbnailChange(emptyBitmap)
        verify(observer, times(1)).onAppPermissionRequest(permissionRequest)
        verify(observer, times(1)).onContentPermissionRequest(permissionRequest)
        verify(observer, times(1)).onCancelContentPermissionRequest(permissionRequest)
        verify(observer, times(1)).onOpenWindowRequest(windowRequest)
        verify(observer, times(1)).onCloseWindowRequest(windowRequest)
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `all HitResults are supported`() {
        val session = spy(DummyEngineSession())
        val observer = mock(EngineSession.Observer::class.java)
        session.register(observer)

        var hitResult: HitResult = HitResult.UNKNOWN("file://foobaz")
        session.notifyInternalObservers { onLongPress(hitResult) }
        verify(observer, times(1)).onLongPress(hitResult)

        hitResult = HitResult.EMAIL("mailto:asa@mozilla.com")
        session.notifyInternalObservers { onLongPress(hitResult) }
        verify(observer, times(1)).onLongPress(hitResult)

        hitResult = HitResult.PHONE("tel:+1234567890")
        session.notifyInternalObservers { onLongPress(hitResult) }
        verify(observer, times(1)).onLongPress(hitResult)

        hitResult = HitResult.IMAGE_SRC("file.png", "https://mozilla.org")
        session.notifyInternalObservers { onLongPress(hitResult) }
        verify(observer, times(1)).onLongPress(hitResult)

        hitResult = HitResult.IMAGE("file.png")
        session.notifyInternalObservers { onLongPress(hitResult) }
        verify(observer, times(1)).onLongPress(hitResult)

        hitResult = HitResult.AUDIO("file.mp3")
        session.notifyInternalObservers { onLongPress(hitResult) }
        verify(observer, times(1)).onLongPress(hitResult)

        hitResult = HitResult.GEO("geo:1,-1")
        session.notifyInternalObservers { onLongPress(hitResult) }
        verify(observer, times(1)).onLongPress(hitResult)

        hitResult = HitResult.VIDEO("file.mp4")
        session.notifyInternalObservers { onLongPress(hitResult) }
        verify(observer, times(1)).onLongPress(hitResult)
    }

    @Test
    fun `registered observer will be notified about download`() {
        val session = spy(DummyEngineSession())

        val observer = mock(EngineSession.Observer::class.java)
        session.register(observer)

        session.notifyInternalObservers {
            onExternalResource(
                url = "https://download.mozilla.org",
                fileName = "firefox.apk",
                contentLength = 1927392,
                contentType = "application/vnd.android.package-archive",
                cookie = "PHPSESSID=298zf09hf012fh2; csrftoken=u32t4o3tb3gg43; _gat=1;",
                userAgent = "Components/1.0")
        }

        verify(observer).onExternalResource(
            url = "https://download.mozilla.org",
            fileName = "firefox.apk",
            contentLength = 1927392,
            contentType = "application/vnd.android.package-archive",
            cookie = "PHPSESSID=298zf09hf012fh2; csrftoken=u32t4o3tb3gg43; _gat=1;",
            userAgent = "Components/1.0"
        )
    }

    @Test
    fun `tracking protection policies have correct categories`() {

        assertEquals(TrackingProtectionPolicy.RECOMMENDED, TrackingProtectionPolicy.recommended())

        val strictPolicy = TrackingProtectionPolicy.strict()

        assertEquals(
            strictPolicy.trackingCategories.sumBy { it.id },
            TrackingCategory.STRICT.id
        )

        assertEquals(strictPolicy.cookiePolicy.id, CookiePolicy.ACCEPT_NON_TRACKERS.id)

        val nonePolicy = TrackingProtectionPolicy.none()

        assertEquals(
            nonePolicy.trackingCategories.sumBy { it.id },
            TrackingCategory.NONE.id
        )

        assertEquals(nonePolicy.cookiePolicy.id, CookiePolicy.ACCEPT_ALL.id)

        val newPolicy = TrackingProtectionPolicy.select(
            trackingCategories = arrayOf(
                TrackingCategory.AD,
                TrackingCategory.SOCIAL,
                TrackingCategory.ANALYTICS,
                TrackingCategory.CONTENT,
                TrackingCategory.CRYPTOMINING,
                TrackingCategory.FINGERPRINTING,
                TrackingCategory.TEST
            )
        )

        assertEquals(
            newPolicy.trackingCategories.sumBy { it.id },
            arrayOf(
                TrackingCategory.AD,
                TrackingCategory.SOCIAL,
                TrackingCategory.ANALYTICS,
                TrackingCategory.CONTENT,
                TrackingCategory.CRYPTOMINING,
                TrackingCategory.FINGERPRINTING,
                TrackingCategory.TEST
            ).sumBy { it.id })
    }

    @Test
    fun `tracking protection policies can be specified for session type`() {
        val all = TrackingProtectionPolicy.strict()
        val selected = TrackingProtectionPolicy.select(
            trackingCategories = arrayOf(TrackingCategory.AD)

        )

        // Tracking protection policies should be applied to all sessions by default
        assertTrue(all.useForPrivateSessions)
        assertTrue(all.useForRegularSessions)
        assertTrue(selected.useForPrivateSessions)
        assertTrue(selected.useForRegularSessions)

        val allForPrivate = TrackingProtectionPolicy.strict().forPrivateSessionsOnly()
        assertTrue(allForPrivate.useForPrivateSessions)
        assertFalse(allForPrivate.useForRegularSessions)

        val selectedForRegular =
            TrackingProtectionPolicy.select(trackingCategories = arrayOf(TrackingCategory.AD))
                .forRegularSessionsOnly()

        assertTrue(selectedForRegular.useForRegularSessions)
        assertFalse(selectedForRegular.useForPrivateSessions)
    }

    @Test
    fun `load flags can be selected`() {
        assertEquals(LoadUrlFlags.NONE, LoadUrlFlags.none().value)
        assertEquals(LoadUrlFlags.ALL, LoadUrlFlags.all().value)
        assertEquals(LoadUrlFlags.EXTERNAL, LoadUrlFlags.external().value)

        assertTrue(LoadUrlFlags.all().contains(LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE).value))
        assertTrue(LoadUrlFlags.all().contains(LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY).value))
        assertTrue(LoadUrlFlags.all().contains(LoadUrlFlags.select(LoadUrlFlags.EXTERNAL).value))
        assertTrue(LoadUrlFlags.all().contains(LoadUrlFlags.select(LoadUrlFlags.ALLOW_POPUPS).value))
        assertTrue(LoadUrlFlags.all().contains(LoadUrlFlags.select(LoadUrlFlags.BYPASS_CLASSIFIER).value))

        val flags = LoadUrlFlags.select(LoadUrlFlags.EXTERNAL)
        assertTrue(flags.contains(LoadUrlFlags.EXTERNAL))
        assertFalse(flags.contains(LoadUrlFlags.ALLOW_POPUPS))
        assertFalse(flags.contains(LoadUrlFlags.BYPASS_CACHE))
        assertFalse(flags.contains(LoadUrlFlags.BYPASS_CLASSIFIER))
        assertFalse(flags.contains(LoadUrlFlags.BYPASS_PROXY))
    }

    @Test
    fun `engine observer has default methods`() {
        val defaultObserver = object : EngineSession.Observer {}

        defaultObserver.onTitleChange("")
        defaultObserver.onLocationChange("")
        defaultObserver.onLongPress(HitResult.UNKNOWN(""))
        defaultObserver.onExternalResource("", "")
        defaultObserver.onDesktopModeChange(true)
        defaultObserver.onSecurityChange(true)
        defaultObserver.onTrackerBlocked(mock())
        defaultObserver.onTrackerBlockingEnabledChange(true)
        defaultObserver.onFindResult(0, 0, false)
        defaultObserver.onFind("text")
        defaultObserver.onExternalResource("", "")
        defaultObserver.onNavigationStateChange()
        defaultObserver.onProgress(123)
        defaultObserver.onLoadingStateChange(true)
        defaultObserver.onThumbnailChange(spy(Bitmap::class.java))
        defaultObserver.onFullScreenChange(true)
        defaultObserver.onAppPermissionRequest(mock(PermissionRequest::class.java))
        defaultObserver.onContentPermissionRequest(mock(PermissionRequest::class.java))
        defaultObserver.onCancelContentPermissionRequest(mock(PermissionRequest::class.java))
        defaultObserver.onOpenWindowRequest(mock(WindowRequest::class.java))
        defaultObserver.onCloseWindowRequest(mock(WindowRequest::class.java))
        defaultObserver.onMediaAdded(mock())
        defaultObserver.onMediaRemoved(mock())
        defaultObserver.onCrash()
    }

    @Test
    fun `engine doesn't notify observers of clear data`() {
        val session = spy(DummyEngineSession())
        val observer = mock(EngineSession.Observer::class.java)
        session.register(observer)

        session.clearData()

        verifyZeroInteractions(observer)
    }

    @Test
    fun `trackingProtectionPolicy contains should work with compound categories`() {
        val recommendedPolicy = TrackingProtectionPolicy.recommended()

        assertTrue(recommendedPolicy.contains(TrackingCategory.RECOMMENDED))
        assertTrue(recommendedPolicy.contains(TrackingCategory.AD))
        assertTrue(recommendedPolicy.contains(TrackingCategory.ANALYTICS))
        assertTrue(recommendedPolicy.contains(TrackingCategory.SOCIAL))
        assertTrue(recommendedPolicy.contains(TrackingCategory.TEST))

        assertFalse(recommendedPolicy.contains(TrackingCategory.FINGERPRINTING))
        assertFalse(recommendedPolicy.contains(TrackingCategory.CRYPTOMINING))
        assertFalse(recommendedPolicy.contains(TrackingCategory.CONTENT))

        val strictPolicy = TrackingProtectionPolicy.strict()

        assertTrue(strictPolicy.contains(TrackingCategory.RECOMMENDED))
        assertTrue(strictPolicy.contains(TrackingCategory.AD))
        assertTrue(strictPolicy.contains(TrackingCategory.ANALYTICS))
        assertTrue(strictPolicy.contains(TrackingCategory.SOCIAL))
        assertTrue(strictPolicy.contains(TrackingCategory.TEST))
        assertTrue(strictPolicy.contains(TrackingCategory.FINGERPRINTING))
        assertTrue(strictPolicy.contains(TrackingCategory.CRYPTOMINING))
        assertTrue(strictPolicy.contains(TrackingCategory.CONTENT))
    }
}

open class DummyEngineSession : EngineSession() {
    override val settings: Settings
        get() = mock(Settings::class.java)

    override fun restoreState(state: EngineSessionState) {}

    override fun saveState(): EngineSessionState { return mock() }

    override fun loadUrl(url: String, flags: LoadUrlFlags) {}

    override fun loadData(data: String, mimeType: String, encoding: String) {}

    override fun stopLoading() {}

    override fun reload() {}

    override fun goBack() {}

    override fun goForward() {}

    override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {}

    override fun disableTrackingProtection() {}

    override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {}

    override fun findAll(text: String) {}

    override fun findNext(forward: Boolean) {}

    override fun clearFindMatches() {}

    override fun exitFullScreenMode() {}

    override fun recoverFromCrash(): Boolean {
        return false
    }

    // Helper method to access the protected method from test cases.
    fun notifyInternalObservers(block: Observer.() -> Unit) {
        notifyObservers(block)
    }
}
