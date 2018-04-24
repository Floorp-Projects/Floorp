/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
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

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onLocationChange("https://www.firefox.com")
        verify(observer).onProgress(25)
        verify(observer).onProgress(100)
        verify(observer).onLoadingStateChange(true)
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

        session.unregister(observer)

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer).onLoadingStateChange(true)
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

        session.close()

        session.notifyInternalObservers { onLocationChange("https://www.firefox.com") }
        session.notifyInternalObservers { onProgress(100) }
        session.notifyInternalObservers { onLoadingStateChange(false) }

        verify(observer).onLocationChange("https://www.mozilla.org")
        verify(observer).onProgress(25)
        verify(observer, never()).onLocationChange("https://www.firefox.com")
        verify(observer, never()).onProgress(100)
        verify(observer).onLoadingStateChange(true)
        verifyNoMoreInteractions(observer)
    }
}

open class DummyEngineSession : EngineSession() {
    override fun loadUrl(url: String) {}

    override fun goBack() {}

    override fun goForward() {}

    // Helper method to access the protected method from test cases.
    fun notifyInternalObservers(block: Observer.() -> Unit) {
        notifyObservers(block)
    }
}