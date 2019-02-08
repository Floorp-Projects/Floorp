/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.doorhanger

import android.annotation.SuppressLint
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.support.annotation.VisibleForTesting
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.PopupWindow

/**
 * A [Doorhanger] is a floating heads-up popup that can be anchored to a view. They are presented to notify the user
 * of something that is important.
 */
class Doorhanger(
    private val view: View,
    private val onDismiss: (() -> Unit)? = null
) {
    private var currentPopup: PopupWindow? = null

    /**
     * Show this doorhanger and anchor it to the given [View].
     */
    fun show(anchor: View): PopupWindow {
        @SuppressLint("InflateParams")
        val wrapper = LayoutInflater.from(anchor.context).inflate(
            R.layout.mozac_ui_doorhanger_wrapper,
            null,
            false)

        // If we have a wrapper then wrap the view in it. Otherwise use the view directly.
        wrapper.findViewById<ViewGroup>(R.id.mozac_ui_doorhanger_content).addView(view)

        return createPopupWindow(wrapper).also {
            currentPopup = it

            it.showAsDropDown(anchor)
        }
    }

    /**
     * Dismiss this doorhanger if it is currently showing.
     */
    fun dismiss() {
        currentPopup?.dismiss()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun createPopupWindow(view: View) = PopupWindow(
        view,
        WindowManager.LayoutParams.WRAP_CONTENT,
        WindowManager.LayoutParams.WRAP_CONTENT
    ).apply {
        setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        isFocusable = true

        setOnDismissListener {
            currentPopup = null
            onDismiss?.invoke()
        }
    }
}
