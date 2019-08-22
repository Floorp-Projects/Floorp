/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.state

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.utils.AllSessionsObserver
import mozilla.components.concept.engine.media.Media
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * A state machine that subscribes to all [Session] instances and watches changes to their [Media] to create an
 * aggregated [MediaState].
 *
 * Other components can subscribe to the state machine to get notified about [MediaState] changes.
 */
object MediaStateMachine : Observable<MediaStateMachine.Observer> by ObserverRegistry() {
    private val logger = Logger("MediaStateMachine")
    private var observer: AllSessionsObserver? = null

    /**
     * The current [MediaState].
     */
    var state: MediaState = MediaState.None
        @VisibleForTesting(otherwise = PRIVATE)
        internal set

    /**
     * Start observing [Session] and their [Media] and create an aggregated [MediaState] that can be observed.
     */
    fun start(sessionManager: SessionManager) {
        observer = AllSessionsObserver(
            sessionManager,
            MediaSessionObserver(this)
        ).also { it.start() }
    }

    /**
     * Stop observing [Session] and their [Media].
     *
     * The [MediaState] will be reset to [MediaState.None]
     */
    fun stop() {
        observer?.stop()
        state = MediaState.None
    }

    @Synchronized
    internal fun transitionTo(state: MediaState) {
        if (this.state == state) {
            // We are already in this state. Nothing to do.
            return
        }

        logger.info("Transitioning from ${this.state} to $state")

        this.state = state

        notifyObservers { onStateChanged(state) }
    }

    /**
     * Interface for observers that are interested in [MediaState] changes.
     */
    interface Observer {
        /**
         * The [MediaState] has changed.
         */
        fun onStateChanged(state: MediaState)
    }
}

private class MediaSessionObserver(
    private val stateMachine: MediaStateMachine
) : AllSessionsObserver.Observer, Media.Observer {
    private val mediaMap = MediaMap()

    override fun onMediaAdded(session: Session, media: List<Media>, added: Media) {
        added.register(this)

        mediaMap.updateSessionMedia(session, media)

        updateState()
    }

    override fun onMediaRemoved(session: Session, media: List<Media>, removed: Media) {
        removed.unregister(this)

        mediaMap.updateSessionMedia(session, media)

        updateState()
    }

    override fun onStateChanged(media: Media, state: Media.State) {
        updateState()
    }

    override fun onRegisteredToSession(session: Session) {
        session.media.forEach { media -> media.register(this) }
        mediaMap.updateSessionMedia(session, session.media)

        updateState()
    }

    override fun onUnregisteredFromSession(session: Session) {
        mediaMap.getAllMedia(session).forEach { media ->
            media.unregister(this)
        }

        mediaMap.updateSessionMedia(session, emptyList())

        updateState()
    }

    private fun updateState() {
        val state = determineNewState()
        stateMachine.transitionTo(state)
    }

    @Suppress("ReturnCount")
    private fun determineNewState(): MediaState {
        val currentState = stateMachine.state

        // If we are in "playing" state and there's still media playing for this session then stay in this state and
        // just update the list of media.
        if (currentState is MediaState.Playing) {
            val media = mediaMap.getPlayingMedia(currentState.session)
            if (media.isNotEmpty()) {
                return MediaState.Playing(currentState.session, media)
            }
        }

        // Check if there's a session that has playing media and emit a "playing" state for it.
        val playingSession = mediaMap.findPlayingSession()
        if (playingSession != null) {
            return MediaState.Playing(
                playingSession.first,
                playingSession.second
            )
        }

        // If we were in "playing" state and the media for this session is now paused then emit a "paused" state.
        if (currentState is MediaState.Playing) {
            val media = mediaMap.getPausedMedia(currentState.session)
            if (media.isNotEmpty()) {
                return MediaState.Paused(currentState.session, media.filter {
                    // We only add paused media to the state that was playing before. If the user
                    // chooses to "resume" playback then we only want to resume this list of media
                    // and not all paused media in the session.
                    currentState.media.contains(it)
                })
            }
        }

        // If we are in "paused" state and the media for this session is still paused then stay in this state and just
        // update the list of media
        if (currentState is MediaState.Paused) {
            val media = mediaMap.getPausedMedia(currentState.session)
            if (media.isNotEmpty()) {
                return MediaState.Paused(currentState.session, media)
            }
        }

        return MediaState.None
    }
}
