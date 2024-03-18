/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.store

import mozilla.components.lib.state.State

/**
 * UI state of the debug drawer feature.
 *
 * @property drawerStatus The [DrawerStatus] indicating the physical state of the drawer.
 */
data class DebugDrawerState(
    val drawerStatus: DrawerStatus = DrawerStatus.Closed,
) : State
