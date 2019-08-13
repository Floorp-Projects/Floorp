/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.focus

/**
 * A controller that knows how to request and abandon audio focus.
 */
internal interface AudioFocusController {
    fun request(): Int
    fun abandon()
}
