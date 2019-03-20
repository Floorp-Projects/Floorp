package mozilla.components.browser.engine.gecko.media

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.media.Media
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.MediaElement
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GeckoMediaDelegateTest {
    @Test
    fun `Added MediaElement is wrapped in GeckoMedia and forwarded to observer`() {
        val engineSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))

        var observedMedia: Media? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onMediaAdded(media: Media) {
                observedMedia = media
            }
        })

        val mediaElement: MediaElement = mock()
        engineSession.geckoSession.mediaDelegate!!.onMediaAdd(mock(), mediaElement)

        assertNotNull(observedMedia!!)

        observedMedia!!.controller.play()
        verify(mediaElement).play()
    }

    @Test
    fun `WHEN MediaElement is removed THEN previously added GeckoMedia is used to notify observer`() {
        val engineSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))

        var addedMedia: Media? = null
        var removedMedia: Media? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onMediaAdded(media: Media) {
                addedMedia = media
            }

            override fun onMediaRemoved(media: Media) {
                removedMedia = media
            }
        })

        val mediaElement: MediaElement = mock()
        engineSession.geckoSession.mediaDelegate!!.onMediaAdd(mock(), mediaElement)
        engineSession.geckoSession.mediaDelegate!!.onMediaRemove(mock(), mediaElement)

        assertNotNull(addedMedia!!)
        assertNotNull(removedMedia!!)
        assertEquals(addedMedia, removedMedia)
    }

    @Test
    fun `WHEN unknown media is removed THEN observer is not notified`() {
        val engineSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))

        var onMediaRemovedExecuted = false

        engineSession.register(object : EngineSession.Observer {
            override fun onMediaRemoved(media: Media) {
                onMediaRemovedExecuted = true
            }
        })

        val mediaElement: MediaElement = mock()
        engineSession.geckoSession.mediaDelegate!!.onMediaRemove(mock(), mediaElement)

        assertFalse(onMediaRemovedExecuted)
    }
}
