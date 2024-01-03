/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.store

import mozilla.components.lib.state.Action

/**
 * [Action] implementation related to [DebugDrawerStore].
 */
sealed class DebugDrawerAction : Action {

    /**
     * [DebugDrawerAction] fired when the user opens the drawer.
     */
    object DrawerOpened : DebugDrawerAction()

    /**
     * [DebugDrawerAction] fired when the user closes the drawer.
     */
    object DrawerClosed : DebugDrawerAction()
}
