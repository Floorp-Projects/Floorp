/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import android.content.Context

private const val MIN_COLUMN_WIDTH_DP = 180

/**
 * Returns the number of grid columns we can fit on the screen in the tabs tray.
 */
internal val Context.numberOfGridColumns: Int
    get() {
        val displayMetrics = resources.displayMetrics
        val screenWidthDp = displayMetrics.widthPixels / displayMetrics.density
        return (screenWidthDp / MIN_COLUMN_WIDTH_DP).toInt().coerceAtLeast(2)
    }
