/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import org.junit.Assert.assertEquals
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class EngineSessionTest {
    private val unknownHitResult = HitResult.UNKNOWN("file://foobar")

    @Test
    fun `registered observers will be notified`() {
        val session = spy(DummyEngineSession())

        val observer = mock(EngineSession.Observer::class.java)
        session.register(observer)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked("tracker") }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(true) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onLocationChange("https://www.firefox.com")
        verify(observer).onProgress(25)
        verify(observer).onProgress(100)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer).onTrackerBlockingEnabledChange(true)
        verify(observer).onTrackerBlocked("tracker")
        verify(observer).onLongPress(unknownHitResult)
        verify(observer).onDesktopModeChange(true)
        verify(observer).onFind("search")
        verify(observer).onFindResult(0, 1, true)
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer will not be notified after calling unregister`() {
        val session = spy(DummyEngineSession())
        val observer = mock(EngineSession.Observer::class.java)
        val otherHitResult = HitResult.UNKNOWN("file://foobaz")
        session.register(observer)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked("tracker") }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(true) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }

        session.unregister(observer)

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }
        session.notifyInternalObservers { onSecurityChange(false, "", "") }
        session.notifyInternalObservers { onTrackerBlocked("tracker2") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(false) }
        session.notifyInternalObservers { onLongPress(otherHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(false) }
        session.notifyInternalObservers { onFind("search2") }
        session.notifyInternalObservers { onFindResult(0, 1, false) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer).onTrackerBlockingEnabledChange(true)
        verify(observer).onTrackerBlocked("tracker")
        verify(observer).onLongPress(unknownHitResult)
        verify(observer).onDesktopModeChange(true)
        verify(observer).onFind("search")
        verify(observer).onFindResult(0, 1, true)
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer, never()).onLoadingStateChange(false)
        verify(observer, never()).onSecurityChange(false, "", "")
        verify(observer, never()).onTrackerBlockingEnabledChange(false)
        verify(observer, never()).onTrackerBlocked("tracker2")
        verify(observer, never()).onLongPress(otherHitResult)
        verify(observer, never()).onDesktopModeChange(false)
        verify(observer, never()).onFind("search2")
        verify(observer, never()).onFindResult(0, 1, false)
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observers will not be notified after calling unregisterObservers`() {
        val session = spy(DummyEngineSession())
        val observer = mock(EngineSession.Observer::class.java)
        val otherObserver = mock(EngineSession.Observer::class.java)
        val otherHitResult = HitResult.UNKNOWN("file://foobaz")
        session.register(observer)
        session.register(otherObserver)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked("tracker") }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(true) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }

        session.unregisterObservers()

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }
        session.notifyInternalObservers { onSecurityChange(false, "", "") }
        session.notifyInternalObservers { onTrackerBlocked("tracker2") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(false) }
        session.notifyInternalObservers { onLongPress(otherHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(false) }
        session.notifyInternalObservers { onFind("search2") }
        session.notifyInternalObservers { onFindResult(0, 1, false) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer).onTrackerBlockingEnabledChange(true)
        verify(observer).onTrackerBlocked("tracker")
        verify(observer).onLongPress(unknownHitResult)
        verify(observer).onDesktopModeChange(true)
        verify(observer).onFind("search")
        verify(observer).onFindResult(0, 1, true)
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer, never()).onLoadingStateChange(false)
        verify(observer, never()).onSecurityChange(false, "", "")
        verify(observer, never()).onTrackerBlockingEnabledChange(false)
        verify(observer, never()).onTrackerBlocked("tracker2")
        verify(observer, never()).onLongPress(otherHitResult)
        verify(observer, never()).onDesktopModeChange(false)
        verify(observer, never()).onFind("search2")
        verify(observer, never()).onFindResult(0, 1, false)
        verify(otherObserver, never()).onLocationChange("https://www.firefox.com")
        verify(otherObserver, never()).onProgress(100)
        verify(otherObserver, never()).onLoadingStateChange(false)
        verify(otherObserver, never()).onSecurityChange(false, "", "")
        verify(otherObserver, never()).onTrackerBlockingEnabledChange(false)
        verify(otherObserver, never()).onTrackerBlocked("tracker2")
        verify(otherObserver, never()).onLongPress(otherHitResult)
        verify(otherObserver, never()).onDesktopModeChange(false)
        verify(otherObserver, never()).onFind("search2")
        verify(otherObserver, never()).onFindResult(0, 1, false)
    }

    @Test
    fun `observer will not be notified after session is closed`() {
        val session = spy(DummyEngineSession())
        val observer = mock(EngineSession.Observer::class.java)
        val otherHitResult = HitResult.UNKNOWN("file://foobaz")
        session.register(observer)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked("tracker") }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(true) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }

        session.close()

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }
        session.notifyInternalObservers { onSecurityChange(false, "", "") }
        session.notifyInternalObservers { onTrackerBlocked("tracker2") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(false) }
        session.notifyInternalObservers { onLongPress(otherHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(false) }
        session.notifyInternalObservers { onFind("search2") }
        session.notifyInternalObservers { onFindResult(0, 1, false) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer).onTrackerBlockingEnabledChange(true)
        verify(observer).onTrackerBlocked("tracker")
        verify(observer).onLongPress(unknownHitResult)
        verify(observer).onDesktopModeChange(true)
        verify(observer).onFind("search")
        verify(observer).onFindResult(0, 1, true)
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer, never()).onLoadingStateChange(false)
        verify(observer, never()).onSecurityChange(false, "", "")
        verify(observer, never()).onTrackerBlockingEnabledChange(false)
        verify(observer, never()).onTrackerBlocked("tracker2")
        verify(observer, never()).onLongPress(otherHitResult)
        verify(observer, never()).onDesktopModeChange(false)
        verify(observer, never()).onFind("search2")
        verify(observer, never()).onFindResult(0, 1, false)
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `registered observers are instance specific`() {
        val session = spy(DummyEngineSession())
        val otherSession = spy(DummyEngineSession())

        val observer = mock(EngineSession.Observer::class.java)
        session.register(observer)

        otherSession.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        otherSession.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        otherSession.notifyInternalObservers { onProgress(25) }
        otherSession.notifyInternalObservers { onLoadingStateChange(true) }
        otherSession.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        otherSession.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        otherSession.notifyInternalObservers { onTrackerBlocked("tracker") }
        otherSession.notifyInternalObservers { onLongPress(unknownHitResult) }
        otherSession.notifyInternalObservers { onDesktopModeChange(true) }
        otherSession.notifyInternalObservers { onFind("search") }
        otherSession.notifyInternalObservers { onFindResult(0, 1, true) }
        verify(observer, never()).onLocationChange("https://www.mozilla.org")
        verify(observer, never()).onProgress(25)
        verify(observer, never()).onLoadingStateChange(true)
        verify(observer, never()).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer, never()).onTrackerBlockingEnabledChange(true)
        verify(observer, never()).onTrackerBlocked("tracker")
        verify(observer, never()).onLongPress(unknownHitResult)
        verify(observer, never()).onDesktopModeChange(true)
        verify(observer, never()).onFind("search")
        verify(observer, never()).onFindResult(0, 1, true)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }
        session.notifyInternalObservers { onTrackerBlockingEnabledChange(true) }
        session.notifyInternalObservers { onTrackerBlocked("tracker") }
        session.notifyInternalObservers { onLongPress(unknownHitResult) }
        session.notifyInternalObservers { onDesktopModeChange(false) }
        session.notifyInternalObservers { onFind("search") }
        session.notifyInternalObservers { onFindResult(0, 1, true) }
        verify(observer, times(1)).onLocationChange("https://www.mozilla.org")
        verify(observer, times(1)).onProgress(25)
        verify(observer, times(1)).onLoadingStateChange(true)
        verify(observer, times(1)).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer, times(1)).onTrackerBlockingEnabledChange(true)
        verify(observer, times(1)).onTrackerBlocked("tracker")
        verify(observer, times(1)).onLongPress(unknownHitResult)
        verify(observer, times(1)).onDesktopModeChange(false)
        verify(observer, times(1)).onFind("search")
        verify(observer, times(1)).onFindResult(0, 1, true)
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
    fun `tracking policies have correct categories`() {
        assertEquals(TrackingProtectionPolicy.ALL, TrackingProtectionPolicy.all().categories)
        assertEquals(TrackingProtectionPolicy.NONE, TrackingProtectionPolicy.none().categories)
        assertEquals(TrackingProtectionPolicy.ALL, TrackingProtectionPolicy.select(
                TrackingProtectionPolicy.AD,
                TrackingProtectionPolicy.ANALYTICS,
                TrackingProtectionPolicy.CONTENT,
                TrackingProtectionPolicy.SOCIAL).categories)
    }

    @Test
    fun `engine observer has default methods`() {
        val defaultObserver = object : EngineSession.Observer {}

        defaultObserver.onTitleChange("")
        defaultObserver.onLocationChange("")
        defaultObserver.onLongPress(HitResult.UNKNOWN(""))
        defaultObserver.onExternalResource("")
        defaultObserver.onDesktopModeChange(true)
        defaultObserver.onSecurityChange(true)
        defaultObserver.onTrackerBlocked("")
        defaultObserver.onTrackerBlockingEnabledChange(true)
        defaultObserver.onFindResult(0, 0, false)
        defaultObserver.onFind("text")
        defaultObserver.onExternalResource("")
        defaultObserver.onNavigationStateChange()
        defaultObserver.onProgress(123)
        defaultObserver.onLoadingStateChange(true)
    }
}

open class DummyEngineSession : EngineSession() {
    override val settings: Settings
        get() = mock(Settings::class.java)

    override fun restoreState(state: Map<String, Any>) {}

    override fun saveState(): Map<String, Any> { return emptyMap() }

    override fun loadUrl(url: String) {}

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

    // Helper method to access the protected method from test cases.
    fun notifyInternalObservers(block: Observer.() -> Unit) {
        notifyObservers(block)
    }
}