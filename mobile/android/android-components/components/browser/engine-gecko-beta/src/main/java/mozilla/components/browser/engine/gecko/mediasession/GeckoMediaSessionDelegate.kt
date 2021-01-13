/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.mediasession

import android.graphics.Bitmap
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.browser.engine.gecko.await
import mozilla.components.concept.engine.mediasession.MediaSession
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MediaSession as GeckoViewMediaSession

internal class GeckoMediaSessionDelegate(
    private val engineSession: GeckoEngineSession
) : GeckoViewMediaSession.Delegate {

    override fun onActivated(geckoSession: GeckoSession, mediaSession: GeckoViewMediaSession) {
        engineSession.notifyObservers {
            onMediaActivated(GeckoMediaSessionController(mediaSession))
        }
    }

    override fun onDeactivated(session: GeckoSession, mediaSession: GeckoViewMediaSession) {
        engineSession.notifyObservers {
            onMediaDeactivated()
        }
    }

    override fun onMetadata(
        session: GeckoSession,
        mediaSession: GeckoViewMediaSession,
        metaData: GeckoViewMediaSession.Metadata
    ) {
        val getArtwork: (suspend (Int) -> Bitmap?)? = metaData.artwork?.let {
            { size -> it.getBitmap(size).await() }
        }

        engineSession.notifyObservers {
            onMediaMetadataChanged(
                MediaSession.Metadata(metaData.title, metaData.artist, metaData.album, getArtwork)
            )
        }
    }

    override fun onFeatures(
        session: GeckoSession,
        mediaSession: GeckoViewMediaSession,
        features: Long
    ) {
        engineSession.notifyObservers {
            onMediaFeatureChanged(MediaSession.Feature(features))
        }
    }

    override fun onPlay(session: GeckoSession, mediaSession: GeckoViewMediaSession) {
        engineSession.notifyObservers {
            onMediaPlaybackStateChanged(MediaSession.PlaybackState.PLAYING)
        }
    }

    override fun onPause(session: GeckoSession, mediaSession: GeckoViewMediaSession) {
        engineSession.notifyObservers {
            onMediaPlaybackStateChanged(MediaSession.PlaybackState.PAUSED)
        }
    }

    override fun onStop(session: GeckoSession, mediaSession: GeckoViewMediaSession) {
        engineSession.notifyObservers {
            onMediaPlaybackStateChanged(MediaSession.PlaybackState.STOPPED)
        }
    }

    override fun onPositionState(
        session: GeckoSession,
        mediaSession: GeckoViewMediaSession,
        positionState: GeckoViewMediaSession.PositionState
    ) {
        engineSession.notifyObservers {
            onMediaPositionStateChanged(
                MediaSession.PositionState(positionState.duration, positionState.position,
                    positionState.playbackRate)
            )
        }
    }

    override fun onFullscreen(
        session: GeckoSession,
        mediaSession: GeckoViewMediaSession,
        enabled: Boolean,
        elementMetaData: GeckoViewMediaSession.ElementMetadata?
    ) {
        val sessionElementMetaData =
            elementMetaData?.let {
                MediaSession.ElementMetadata(
                    elementMetaData.source,
                    elementMetaData.duration,
                    elementMetaData.width,
                    elementMetaData.height,
                    elementMetaData.audioTrackCount,
                    elementMetaData.videoTrackCount
                )
            }

        engineSession.notifyObservers {
            onMediaFullscreenChanged(enabled, sessionElementMetaData)
        }
    }
}
