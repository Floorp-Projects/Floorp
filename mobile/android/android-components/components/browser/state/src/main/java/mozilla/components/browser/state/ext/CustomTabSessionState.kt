/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.ext

import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState

internal fun CustomTabSessionState.toTab(): TabSessionState {
    return TabSessionState(
        id = id,
        content = content,
        trackingProtection = trackingProtection,
        engineState = engineState,
        extensionState = extensionState,
        contextId = contextId,
    )
}
