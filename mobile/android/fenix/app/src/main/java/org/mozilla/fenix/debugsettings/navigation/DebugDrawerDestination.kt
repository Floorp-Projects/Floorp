/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.navigation

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable

/**
 * A navigation destination for screens within the Debug Drawer.
 *
 * @property route The unique route used to navigate to the destination. This string can also contain
 * optional parameters for arguments or deep linking.
 * @property title The string ID of the destination's title.
 * @property onClick Invoked when the destination is clicked to be navigated to.
 * @property content The destination's [Composable].
 */
data class DebugDrawerDestination(
    val route: String,
    @StringRes val title: Int,
    val onClick: () -> Unit,
    val content: @Composable () -> Unit,
)
