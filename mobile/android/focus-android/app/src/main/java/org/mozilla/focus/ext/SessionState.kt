/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.SessionState

/**
 * Returns this [SessionState] cast to [CustomTabSessionState] if possible. Otherwise returns `null`.
 */
fun SessionState.ifCustomTab(): CustomTabSessionState? {
    if (this is CustomTabSessionState) {
        return this
    }
    return null
}

/**
 * Returns `true` if this [SessionState] is a custom tab (an instance of [CustomTabSessionState]).
 */
fun SessionState.isCustomTab(): Boolean {
    return this is CustomTabSessionState
}
