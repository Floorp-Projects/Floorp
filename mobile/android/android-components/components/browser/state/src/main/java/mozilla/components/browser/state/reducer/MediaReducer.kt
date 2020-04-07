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
            is MediaAction.AddMediaAction -> state.updateMediaList(action.tabId) { list ->
                list + action.media
            }

            is MediaAction.RemoveMediaAction -> state.updateMediaList(action.tabId) { list ->
                list.filter { element -> element.id != action.media.id }
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

            is MediaAction.UpdateMediaVolumeAction -> state.updateMediaElement(action.tabId, action.mediaId) {
                it.copy(volume = action.volume)
            }

            is MediaAction.UpdateMediaAggregateAction -> state.copy(
                media = state.media.copy(aggregate = action.aggregate)
            )
        }
    }
}

private fun BrowserState.updateMediaList(
    tabId: String,
    update: (List<MediaState.Element>) -> List<MediaState.Element>
): BrowserState {
    val elements = update(media.elements[tabId] ?: emptyList())

    return copy(media = media.copy(
        elements = (media.elements + (tabId to elements)).filterValues { it.isNotEmpty() }
    ))
}

private fun BrowserState.updateMediaElement(
    tabId: String,
    mediaId: String,
    update: (MediaState.Element) -> MediaState.Element
): BrowserState {
    return copy(media = media.copy(
        elements = media.elements.mapValues { entry ->
            if (entry.key == tabId) {
                entry.value.map { element ->
                    if (element.id == mediaId) {
                        update(element)
                    } else {
                        element
                    }
                }
            } else {
                entry.value
            }
        }
    ))
}
