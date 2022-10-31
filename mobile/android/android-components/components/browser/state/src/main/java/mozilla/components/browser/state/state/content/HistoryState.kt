/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.content

import mozilla.components.concept.engine.history.HistoryItem

/**
 * Value type that represents browser history.
 *
 * @property items All the items in the browser history.
 * @property currentIndex The index of the currently selected [HistoryItem].
 * If this is equal to lastIndex, then there are no pages to go "forward" to.
 * If this is 0, then there are no pages to go "back" to.
 */
data class HistoryState(
    val items: List<HistoryItem> = emptyList(),
    val currentIndex: Int = 0,
)
