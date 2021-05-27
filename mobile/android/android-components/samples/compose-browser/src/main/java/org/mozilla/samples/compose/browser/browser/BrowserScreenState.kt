/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.browser

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import mozilla.components.lib.state.State

/**
 * The state the browser screen is in.
 */
@Parcelize
data class BrowserScreenState(
    val editMode: Boolean = false,
    val editText: String? = null
) : State, Parcelable
