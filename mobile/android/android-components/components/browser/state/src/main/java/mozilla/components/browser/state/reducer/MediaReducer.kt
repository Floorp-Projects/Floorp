/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.SessionState

internal object MediaReducer {
    /**
     * [MediaAction] Reducer function for modifying a specific [MediaState] of a [SessionState].
     */
    fun reduce(state: BrowserState, action: MediaAction): BrowserState {
        return when (action) {
            is MediaAction.AddMediaAction -> state.updateMediaList(action.tabId) {
                it.add(action.media)
            }

            is MediaAction.RemoveMediaAction -> state.updateMediaList(action.tabId) {
                it.removeAll { element -> element.id == action.media.id }
            }

            is MediaAction.RemoveTabMediaAction -> {
                val tabIdSet = HashSet<String>().apply { addAll(action.tabIds) }
                state.copy(
                    media = state.media.copy(
                        elements = state.media.elements.filterKeys { it !in tabIdSet }
                    )
                )
            }

            is MediaAction.UpdateMediaStateAction -> state.updateMediaElement(action.tabId, action.mediaId) {
                it.copy(state = action.state)
            }

            is MediaAction.UpdateMediaPlaybackStateAction -> state.updateMediaElement(action.tabId, action.mediaId) {
                it.copy(playbackState = action.playbackState)
            }

            is MediaAction.UpdateMediaMetadataAction -> state.updateMediaElement(action.tabId, action.mediaId) {
                it.copy(metadata = action.metadata)
            }

            is MediaAction.UpdateMediaAggregateAction -> state.copy(
                media = state.media.copy(aggregate = action.aggregate)
            )
        }
    }
}

private fun BrowserState.updateMediaList(
    tabId: String,
    update: (MutableList<MediaState.Element>) -> Unit
): BrowserState {
    return copy(media = media.copy(
        elements = media.elements.toMutableMap().apply {
            val list = this[tabId]?.toMutableList() ?: mutableListOf()
            update(list)

            if (list.isEmpty()) {
                remove(tabId)
            } else {
                put(tabId, list)
            }
        }
    ))
}

private fun BrowserState.updateMediaElement(
    tabId: String,
    mediaId: String,
    update: (MediaState.Element) -> MediaState.Element
): BrowserState {
    return copy(media = media.copy(
        elements = media.elements.toMutableMap().apply {
            val list = this[tabId]
            if (list != null) {
                this[tabId] = list.map {
                    if (it.id == mediaId) {
                        update(it)
                    } else {
                        it
                    }
                }
            }
        }
    ))
}
