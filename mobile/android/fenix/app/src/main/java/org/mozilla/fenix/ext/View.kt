/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ext

import android.graphics.Rect
import android.os.Build
import android.view.TouchDelegate
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import androidx.annotation.Dimension
import androidx.annotation.Dimension.Companion.DP
import androidx.annotation.VisibleForTesting
import androidx.core.view.WindowInsetsCompat
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.utils.ext.bottom
import org.mozilla.fenix.R

fun View.increaseTapArea(@Dimension(unit = DP) extraDps: Int) {
    val dips = extraDps.dpToPx(resources.displayMetrics)
    val parent = this.parent as View
    parent.post {
        val touchRect = Rect()
        getHitRect(touchRect)
        touchRect.inset(-dips, -dips)
        parent.touchDelegate = TouchDelegate(touchRect, this)
    }
}

/**
 * Increase tap area only vertically.
 *
 * @param extraDps the extra dps that's wanted to be added on top and bottom of the view
 */
fun View.increaseTapAreaVertically(@Dimension(unit = DP) extraDps: Int) {
    val dips = extraDps.dpToPx(resources.displayMetrics)
    val parent = this.parent as View
    parent.post {
        val touchArea = Rect()
        getHitRect(touchArea)
        touchArea.top -= dips
        touchArea.bottom += dips
        parent.touchDelegate = TouchDelegate(touchArea, this)
    }
}

fun View.removeTouchDelegate() {
    val parent = this.parent as View
    parent.post {
        parent.touchDelegate = null
    }
}

/**
 * Sets the new a11y parent.
 */
fun View.setNewAccessibilityParent(newParent: View) {
    this.accessibilityDelegate = object : View.AccessibilityDelegate() {
        override fun onInitializeAccessibilityNodeInfo(
            host: View,
            info: AccessibilityNodeInfo,
        ) {
            super.onInitializeAccessibilityNodeInfo(host, info)
            info.setParent(newParent)
        }
    }
}

/**
 * Updates the a11y collection item info for an item in a list.
 */
fun View.updateAccessibilityCollectionItemInfo(
    rowIndex: Int,
    columnIndex: Int,
    isSelected: Boolean,
    rowSpan: Int = 1,
    columnSpan: Int = 1,
) {
    this.accessibilityDelegate = object : View.AccessibilityDelegate() {
        override fun onInitializeAccessibilityNodeInfo(
            host: View,
            info: AccessibilityNodeInfo,
        ) {
            super.onInitializeAccessibilityNodeInfo(host, info)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                info.collectionItemInfo =
                    AccessibilityNodeInfo.CollectionItemInfo(
                        rowIndex,
                        rowSpan,
                        columnIndex,
                        columnSpan,
                        false,
                        isSelected,
                    )
            } else {
                @Suppress("DEPRECATION")
                AccessibilityNodeInfo.CollectionItemInfo.obtain(
                    rowIndex,
                    rowSpan,
                    columnIndex,
                    columnSpan,
                    false,
                    isSelected,
                )
            }
        }
    }
}

/**
 * Updates the a11y collection info for a list.
 */
fun View.updateAccessibilityCollectionInfo(
    rowCount: Int,
    columnCount: Int,
) {
    this.accessibilityDelegate = object : View.AccessibilityDelegate() {
        override fun onInitializeAccessibilityNodeInfo(
            host: View,
            info: AccessibilityNodeInfo,
        ) {
            super.onInitializeAccessibilityNodeInfo(host, info)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                info.collectionInfo = AccessibilityNodeInfo.CollectionInfo(
                    rowCount,
                    columnCount,
                    false,
                )
            } else {
                @Suppress("DEPRECATION")
                info.collectionInfo = AccessibilityNodeInfo.CollectionInfo.obtain(
                    rowCount,
                    columnCount,
                    false,
                )
            }
        }
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
    return Rect(
        locationOnScreen[0],
        locationOnScreen[1],
        locationOnScreen[0] + width,
        locationOnScreen[1] + height,
    )
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
        resources.getDimensionPixelSize(R.dimen.minimum_keyboard_height)
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
        keyboardHeight -= it.bottom()
    }

    return keyboardHeight
}

/**
 * The assumed minimum height of the keyboard.
 */
@VisibleForTesting
@Dimension(unit = DP)
internal const val MINIMUM_KEYBOARD_HEIGHT = 100
