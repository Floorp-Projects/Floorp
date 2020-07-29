/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware.sideeffects

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.media.ext.findFullscreenMediaOrientation
import mozilla.components.feature.media.ext.findPlayingSession
import mozilla.components.feature.media.ext.getPausedMedia
import mozilla.components.feature.media.ext.getPlayingMediaIdsForTab
import mozilla.components.feature.media.ext.hasMediaWithAudibleAudio
import mozilla.components.feature.media.ext.hasMediaWithSufficientLongDuration
import mozilla.components.lib.state.Store
import mozilla.components.support.base.coroutines.Dispatchers

private const val DELAY_AGGREGATE_STATE_UPDATE_MS = 100L

internal class MediaAggregateUpdater(
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Cached)
) {
    @VisibleForTesting(otherwise = PRIVATE)
    internal var updateAggregateJob: Job? = null

    fun process(store: Store<BrowserState, BrowserAction>) {
        val aggregate = aggregateNewState(store.state.media)
        if (aggregate != store.state.media.aggregate) {
            // We delay updating the state here and cancel previous jobs in order to batch multiple
            // state changes together before updating the state. In addition to reducing the work load
            // this also allows us to treat multiple pause events as a single pause event that we can
            // resume from.
            updateAggregateJob?.cancel()
            updateAggregateJob = scope.launch {
                delay(DELAY_AGGREGATE_STATE_UPDATE_MS)
                store.dispatch(MediaAction.UpdateMediaAggregateAction(aggregate))
            }
        }
    }

    @Suppress("ReturnCount", "ComplexMethod")
    private fun aggregateNewState(
        mediaState: MediaState
    ): MediaState.Aggregate {
        // If we are in "playing" state and there's still media playing for this session then stay in this state and
        // just update the list of media.
        if (mediaState.aggregate.state == MediaState.State.PLAYING) {
            val media = mediaState.getPlayingMediaIdsForTab(mediaState.aggregate.activeTabId)
            if (media.isNotEmpty()) {
                return MediaState.Aggregate(
                    MediaState.State.PLAYING,
                    mediaState.aggregate.activeTabId,
                    media,
                    mediaState.findFullscreenMediaOrientation()
                )
            }
        }

        // Check if there's a session that has playing media and emit a "playing" state for it.
        val playingSession = mediaState.findPlayingSession()
        if (playingSession != null) {
            val (session, media) = playingSession
            // We only switch to playing state if there's media playing that has a sufficient long
            // duration and audio. Otherwise we let just Gecko play it and do not request audio focus or show
            // a media notification. This will let us ignore short audio effects (Beeep!).
            return if (media.hasMediaWithSufficientLongDuration() && media.hasMediaWithAudibleAudio()) {
                MediaState.Aggregate(
                    MediaState.State.PLAYING,
                    session,
                    media.map { it.id },
                    mediaState.findFullscreenMediaOrientation()
                )
            } else {
                MediaState.Aggregate(MediaState.State.NONE)
            }
        }

        // If we were in "playing" state and the media for this session is now paused then emit a "paused" state.
        if (mediaState.aggregate.state == MediaState.State.PLAYING) {
            val media = mediaState
                .getPausedMedia(mediaState.aggregate.activeTabId)
                .filter { element ->
                    // We only add paused media to the state that was playing before. If the user
                    // chooses to "resume" playback then we only want to resume this list of media
                    // and not all paused media in the session.
                    element.id in mediaState.aggregate.activeMedia
                }
                .map { it.id }

            if (media.isNotEmpty()) {
                return MediaState.Aggregate(
                    MediaState.State.PAUSED,
                    mediaState.aggregate.activeTabId,
                    media,
                    mediaState.findFullscreenMediaOrientation()
                )
            }
        }

        // If we are in "paused" state and the media for this session is still paused then stay in this state and just
        // update the list of media
        if (mediaState.aggregate.state == MediaState.State.PAUSED) {
            val media = mediaState.getPausedMedia(mediaState.aggregate.activeTabId)
            if (media.isNotEmpty()) {
                return MediaState.Aggregate(
                    MediaState.State.PAUSED,
                    mediaState.aggregate.activeTabId,
                    media.map { it.id },
                    mediaState.findFullscreenMediaOrientation()
                )
            }
        }

        return MediaState.Aggregate(MediaState.State.NONE)
    }
}
