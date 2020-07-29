/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.media.Media
import mozilla.components.lib.state.Store

/**
 * [Media.Observer] implementation responsible for dispatching actions updating the state in
 * [BrowserStore].
 */
internal class MediaObserver(
    val media: Media,
    val element: MediaState.Element,
    val store: Store<BrowserState, BrowserAction>,
    val tabId: String
) : Media.Observer {
    override fun onStateChanged(media: Media, state: Media.State) {
        store.dispatch(MediaAction.UpdateMediaStateAction(
            tabId,
            element.id,
            state
        ))
    }
    override fun onPlaybackStateChanged(media: Media, playbackState: Media.PlaybackState) {
        store.dispatch(MediaAction.UpdateMediaPlaybackStateAction(
            tabId,
            element.id,
            playbackState
        ))
    }

    override fun onMetadataChanged(media: Media, metadata: Media.Metadata) {
        store.dispatch(MediaAction.UpdateMediaMetadataAction(
            tabId,
            element.id,
            metadata
        ))
    }

    override fun onVolumeChanged(media: Media, volume: Media.Volume) {
        store.dispatch(MediaAction.UpdateMediaVolumeAction(
            tabId,
            element.id,
            volume
        ))
    }

    override fun onFullscreenChanged(media: Media, fullscreen: Boolean) {
        store.dispatch(MediaAction.UpdateMediaFullscreenAction(
            tabId,
            element.id,
            fullscreen
        ))
    }
}
