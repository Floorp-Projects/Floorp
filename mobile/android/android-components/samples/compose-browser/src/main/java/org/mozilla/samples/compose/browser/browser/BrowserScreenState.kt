/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.browser

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import mozilla.components.lib.state.State

/**
 * The state the browser screen is in.
 *
 * @param editMode Whether the toolbar is in "edit" or "display" mode.
 * @param editText The text in the toolbar that is being edited by the user.
 */
@Parcelize
data class BrowserScreenState(
    val editMode: Boolean = false,
    val editText: String? = null,
    val showTabs: Boolean = false,
) : State, Parcelable
