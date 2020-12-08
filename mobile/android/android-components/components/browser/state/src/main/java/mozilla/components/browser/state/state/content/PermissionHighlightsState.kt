/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.content

/**
 * Value type that represents any information about permissions that should
 * be brought to user's attention.
 *
 * @property isAutoPlayBlocking indicates if the autoplay setting
 * disabled some web content from playing.
 */
data class PermissionHighlightsState(
    val isAutoPlayBlocking: Boolean = false
)
