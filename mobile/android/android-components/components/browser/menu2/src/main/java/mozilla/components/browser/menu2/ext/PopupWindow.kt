/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.view.Gravity
import android.view.View
import android.widget.PopupWindow
import androidx.core.widget.PopupWindowCompat
import mozilla.components.concept.menu.MenuStyle
import mozilla.components.concept.menu.Orientation

/**
 * If parameters passed are a valid configuration attempt to show a [PopupWindow].
 *
 * @return True if the parameters passed are a valid configuration, otherwise false.
 */
internal fun PopupWindow.displayPopup(
    containerView: View,
    anchor: View,
    orientation: Orientation? = null,
    style: MenuStyle? = null,
): Boolean {
    val positioningData =
        inferMenuPositioningData(containerView, anchor, style, orientation) ?: return false

    inputMethodMode = PopupWindow.INPUT_METHOD_NOT_NEEDED

    showAtAnchorLocation(anchor, positioningData)
    return true
}

private fun PopupWindow.showAtAnchorLocation(
    anchor: View,
    positioningData: MenuPositioningData,
) {
    // Apply the best fit animation style based on positioning
    animationStyle = positioningData.animation
    height = positioningData.containerHeight

    PopupWindowCompat.setOverlapAnchor(this, true)
    showAtLocation(anchor, Gravity.NO_GRAVITY, positioningData.x, positioningData.y)
}
