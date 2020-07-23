/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState

fun BrowserState.getMediaStateForSession(sessionId: String): MediaState.State {
    return if (media.aggregate.activeTabId == sessionId) {
        media.aggregate.state
    } else {
        MediaState.State.NONE
    }
}
