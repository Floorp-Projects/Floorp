/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class EngineSessionTest {
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

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onLocationChange("https://www.firefox.com")
        verify(observer).onProgress(25)
        verify(observer).onProgress(100)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer will not be notified after calling unregister`() {
        val session = spy(DummyEngineSession())

        val observer = mock(EngineSession.Observer::class.java)
        session.register(observer)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }

        session.unregister(observer)

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }
        session.notifyInternalObservers { onSecurityChange(false, "", "") }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer, never()).onLoadingStateChange(false)
        verify(observer, never()).onSecurityChange(false, "", "")
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer will not be notified after session is closed`() {
        val session = spy(DummyEngineSession())

        val observer = mock(EngineSession.Observer::class.java)
        session.register(observer)

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        session.notifyInternalObservers { onProgress(25) }
        session.notifyInternalObservers { onLoadingStateChange(true) }
        session.notifyInternalObservers { onSecurityChange(true, "mozilla.org", "issuer") }

        session.close()

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }
        session.notifyInternalObservers { onSecurityChange(false, "", "") }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer).onLoadingStateChange(true)
        verify(observer).onSecurityChange(true, "mozilla.org", "issuer")

        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer, never()).onLoadingStateChange(false)
        verify(observer, never()).onSecurityChange(false, "", "")
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `registered observers are instance specific`() {
        val session = spy(DummyEngineSession())
        val otherSession = spy(DummyEngineSession())

        val observer = mock(EngineSession.Observer::class.java)
        session.register(observer)

        otherSession.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        verify(observer, never()).onLocationChange("https://www.mozilla.org")

        session.notifyInternalObservers { onLocationChange("https://www.mozilla.org") }
        verify(observer, times(1)).onLocationChange("https://www.mozilla.org")
        verifyNoMoreInteractions(observer)
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
}

open class DummyEngineSession : EngineSession() {
    override fun restoreState(state: Map<String, Any>) {}

    override fun saveState(): Map<String, Any> { return emptyMap() }

    override fun loadUrl(url: String) {}

    override fun reload() {}

    override fun goBack() {}

    override fun goForward() {}

    // Helper method to access the protected method from test cases.
    fun notifyInternalObservers(block: Observer.() -> Unit) {
        notifyObservers(block)
    }
}