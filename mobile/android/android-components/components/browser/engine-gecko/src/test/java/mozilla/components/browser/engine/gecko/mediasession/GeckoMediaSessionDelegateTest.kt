package mozilla.components.browser.engine.gecko.mediasession

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.MediaSession as GeckoViewMediaSession

@RunWith(AndroidJUnit4::class)
class GeckoMediaSessionDelegateTest {
    private lateinit var runtime: GeckoRuntime

    @Before
    fun setup() {
        runtime = mock()
        whenever(runtime.settings).thenReturn(mock())
    }

    @Test
    fun `media session activated is forwarded to observer`() {
        val engineSession = GeckoEngineSession(runtime)
        val geckoViewMediaSession: GeckoViewMediaSession = mock()

        var observedController: MediaSession.Controller? = null

        engineSession.register(
            object : EngineSession.Observer {
                override fun onMediaActivated(mediaSessionController: MediaSession.Controller) {
                    observedController = mediaSessionController
                }
            },
        )

        engineSession.geckoSession.mediaSessionDelegate!!.onActivated(mock(), geckoViewMediaSession)

        assertNotNull(observedController)
        observedController!!.play()
        verify(geckoViewMediaSession).play()
    }

    @Test
    fun `media session deactivated is forwarded to observer`() {
        val engineSession = GeckoEngineSession(runtime)
        val geckoViewMediaSession: GeckoViewMediaSession = mock()

        var observedActivated = true

        engineSession.register(
            object : EngineSession.Observer {
                override fun onMediaDeactivated() {
                    observedActivated = false
                }
            },
        )

        engineSession.geckoSession.mediaSessionDelegate!!.onDeactivated(mock(), geckoViewMediaSession)

        assertFalse(observedActivated)
    }

    @Test
    fun `media session metadata is forwarded to observer`() {
        val engineSession = GeckoEngineSession(runtime)
        val geckoViewMediaSession: GeckoViewMediaSession = mock()

        var observedMetadata: MediaSession.Metadata? = null

        engineSession.register(
            object : EngineSession.Observer {
                override fun onMediaMetadataChanged(
                    metadata: MediaSession.Metadata,
                ) {
                    observedMetadata = metadata
                }
            },
        )

        val metadata: GeckoViewMediaSession.Metadata = mock()
        engineSession.geckoSession.mediaSessionDelegate!!.onMetadata(mock(), geckoViewMediaSession, metadata)

        assertNotNull(observedMetadata)
        assertEquals(observedMetadata?.title, metadata.title)
        assertEquals(observedMetadata?.artist, metadata.artist)
        assertEquals(observedMetadata?.album, metadata.album)
        assertEquals(observedMetadata?.getArtwork, metadata.artwork)
    }

    @Test
    fun `media session feature is forwarded to observer`() {
        val engineSession = GeckoEngineSession(runtime)
        val geckoViewMediaSession: GeckoViewMediaSession = mock()

        var observedFeature: MediaSession.Feature? = null

        engineSession.register(
            object : EngineSession.Observer {
                override fun onMediaFeatureChanged(
                    features: MediaSession.Feature,
                ) {
                    observedFeature = features
                }
            },
        )

        engineSession.geckoSession.mediaSessionDelegate!!.onFeatures(mock(), geckoViewMediaSession, 123)

        assertNotNull(observedFeature)
        assertEquals(observedFeature, MediaSession.Feature(123))
    }

    @Test
    fun `media session play state is forwarded to observer`() {
        val engineSession = GeckoEngineSession(runtime)
        val geckoViewMediaSession: GeckoViewMediaSession = mock()

        var observedPlaybackState: MediaSession.PlaybackState? = null

        engineSession.register(
            object : EngineSession.Observer {
                override fun onMediaPlaybackStateChanged(
                    playbackState: MediaSession.PlaybackState,
                ) {
                    observedPlaybackState = playbackState
                }
            },
        )

        engineSession.geckoSession.mediaSessionDelegate!!.onPlay(mock(), geckoViewMediaSession)

        assertNotNull(observedPlaybackState)
        assertEquals(observedPlaybackState, MediaSession.PlaybackState.PLAYING)

        observedPlaybackState = null
        engineSession.geckoSession.mediaSessionDelegate!!.onPause(mock(), geckoViewMediaSession)

        assertNotNull(observedPlaybackState)
        assertEquals(observedPlaybackState, MediaSession.PlaybackState.PAUSED)

        observedPlaybackState = null
        engineSession.geckoSession.mediaSessionDelegate!!.onStop(mock(), geckoViewMediaSession)

        assertNotNull(observedPlaybackState)
        assertEquals(observedPlaybackState, MediaSession.PlaybackState.STOPPED)
    }

    @Test
    fun `media session position state is forwarded to observer`() {
        val engineSession = GeckoEngineSession(runtime)
        val geckoViewMediaSession: GeckoViewMediaSession = mock()

        var observedPositionState: MediaSession.PositionState? = null

        engineSession.register(
            object : EngineSession.Observer {
                override fun onMediaPositionStateChanged(
                    positionState: MediaSession.PositionState,
                ) {
                    observedPositionState = positionState
                }
            },
        )

        val positionState: GeckoViewMediaSession.PositionState = mock()
        engineSession.geckoSession.mediaSessionDelegate!!.onPositionState(mock(), geckoViewMediaSession, positionState)

        assertNotNull(observedPositionState)
        assertEquals(observedPositionState?.duration, positionState.duration)
        assertEquals(observedPositionState?.position, positionState.position)
        assertEquals(observedPositionState?.playbackRate, positionState.playbackRate)
    }

    @Test
    fun `media session fullscreen state is forwarded to observer`() {
        val engineSession = GeckoEngineSession(runtime)
        val geckoViewMediaSession: GeckoViewMediaSession = mock()

        var observedFullscreen: Boolean? = null
        var observedElementMetadata: MediaSession.ElementMetadata? = null

        engineSession.register(
            object : EngineSession.Observer {
                override fun onMediaFullscreenChanged(
                    fullscreen: Boolean,
                    elementMetadata: MediaSession.ElementMetadata?,
                ) {
                    observedFullscreen = fullscreen
                    observedElementMetadata = elementMetadata
                }
            },
        )

        val elementMetadata: GeckoViewMediaSession.ElementMetadata = mock()
        engineSession.geckoSession.mediaSessionDelegate!!.onFullscreen(mock(), geckoViewMediaSession, true, elementMetadata)

        assertNotNull(observedFullscreen)
        assertNotNull(observedElementMetadata)
        assertEquals(observedFullscreen, true)
        assertEquals(observedElementMetadata?.source, elementMetadata.source)
        assertEquals(observedElementMetadata?.duration, elementMetadata.duration)
        assertEquals(observedElementMetadata?.width, elementMetadata.width)
        assertEquals(observedElementMetadata?.height, elementMetadata.height)
        assertEquals(observedElementMetadata?.audioTrackCount, elementMetadata.audioTrackCount)
        assertEquals(observedElementMetadata?.videoTrackCount, elementMetadata.videoTrackCount)
    }
}
