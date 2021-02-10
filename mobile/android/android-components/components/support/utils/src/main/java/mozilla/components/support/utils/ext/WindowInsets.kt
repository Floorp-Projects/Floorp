/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.os.Build
import android.view.WindowInsets

/**
 * Returns the top system window inset in pixels.
 */
fun WindowInsets.top(): Int =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        this.getInsets(WindowInsets.Type.systemBars()).top
    } else {
        @Suppress("DEPRECATION")
        this.systemWindowInsetTop
    }

/**
 * Returns the right system window inset in pixels.
 */
fun WindowInsets.right(): Int =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        this.getInsets(WindowInsets.Type.systemBars()).right
    } else {
        @Suppress("DEPRECATION")
        this.systemWindowInsetRight
    }

/**
 * Returns the left system window inset in pixels.
 */
fun WindowInsets.left(): Int =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        this.getInsets(WindowInsets.Type.systemBars()).left
    } else {
        @Suppress("DEPRECATION")
        this.systemWindowInsetLeft
    }

/**
 * Returns the bottom system window inset in pixels.
 */
fun WindowInsets.bottom(): Int =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        this.getInsets(WindowInsets.Type.systemBars()).bottom
    } else {
        @Suppress("DEPRECATION")
        this.systemWindowInsetBottom
    }
