/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Rect
import android.os.Build
import android.view.TouchDelegate
import android.view.View
import androidx.annotation.Dimension
import androidx.annotation.VisibleForTesting
import androidx.core.view.WindowInsetsCompat
import mozilla.components.support.ktx.android.util.dpToPx

fun View.increaseTapArea(extraDps: Int) {
    val dips = extraDps.dpToPx(resources.displayMetrics)
    val parent = this.parent as View
    parent.post {
        val touchRect = Rect()
        getHitRect(touchRect)
        touchRect.inset(-dips, -dips)
        parent.touchDelegate = TouchDelegate(touchRect, this)
    }
}

fun View.removeTouchDelegate() {
    val parent = this.parent as View
    parent.post {
        parent.touchDelegate = null
    }
}

/**
 * Fills a [Rect] with data about a view's location in the screen.
 *
 * @see View.getLocationOnScreen
 * @see View.getRectWithViewLocation for a version of this that is relative to a window
*/
fun View.getRectWithScreenLocation(): Rect {
    val locationOnScreen = IntArray(2).apply { getLocationOnScreen(this) }
    return Rect(locationOnScreen[0],
        locationOnScreen[1],
        locationOnScreen[0] + width,
        locationOnScreen[1] + height)
}

/**
 * A safer version of [ViewCompat.getRootWindowInsets] that does not throw a NullPointerException
 * if the view is not attached.
 */
fun View.getWindowInsets(): WindowInsetsCompat? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        rootWindowInsets?.let {
            WindowInsetsCompat.toWindowInsetsCompat(it)
        }
    } else {
        null
    }
}

/**
 * Checks if the keyboard is visible
 *
 * Inspired by https://stackoverflow.com/questions/2150078/how-to-check-visibility-of-software-keyboard-in-android
 * API 30 adds a native method for this. We should use it (and a compat method if one
 * is added) when it becomes available
 */
fun View.isKeyboardVisible(): Boolean {
    // Since we have insets in M and above, we don't need to guess what the keyboard height is.
    // Otherwise, we make a guess at the minimum height of the keyboard to account for the
    // navigation bar.
    val minimumKeyboardHeight = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        0
    } else {
        MINIMUM_KEYBOARD_HEIGHT.dpToPx(resources.displayMetrics)
    }
    return getKeyboardHeight() > minimumKeyboardHeight
}

@VisibleForTesting
internal fun View.getWindowVisibleDisplayFrame(): Rect = with(Rect()) {
    getWindowVisibleDisplayFrame(this)
    this
}

@VisibleForTesting
internal fun View.getKeyboardHeight(): Int {
    val windowRect = getWindowVisibleDisplayFrame()
    val statusBarHeight = windowRect.top
    var keyboardHeight = rootView.height - (windowRect.height() + statusBarHeight)
    getWindowInsets()?.let {
        keyboardHeight -= it.stableInsetBottom
    }

    return keyboardHeight
}

/**
 * The assumed minimum height of the keyboard.
 */
@VisibleForTesting
@Dimension(unit = Dimension.DP)
internal const val MINIMUM_KEYBOARD_HEIGHT = 100
