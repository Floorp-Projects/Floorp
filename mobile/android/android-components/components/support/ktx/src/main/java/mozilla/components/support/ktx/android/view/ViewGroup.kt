/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.view.View
import android.view.ViewGroup
import androidx.core.view.forEach

/**
 * Performs the given action on each View in this ViewGroup.
 */
@Deprecated("Use Android KTX instead",
    ReplaceWith("forEach(action)", "androidx.core.view.forEach"))
inline fun ViewGroup.forEach(action: (View) -> Unit) = forEach(action)
