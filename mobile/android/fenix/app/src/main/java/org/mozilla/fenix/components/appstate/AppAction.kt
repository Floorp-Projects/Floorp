/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.lib.state.Action
import org.mozilla.fenix.components.AppStore

/**
 * [Action] implementation related to [AppStore].
 */
sealed class AppAction : Action {
    data class UpdateInactiveExpanded(val expanded: Boolean) : AppAction()
}
