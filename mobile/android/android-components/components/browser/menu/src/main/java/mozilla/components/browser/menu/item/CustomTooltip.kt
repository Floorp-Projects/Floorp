/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.annotation.SuppressLint
import android.text.TextUtils
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.View.OnLongClickListener
import android.view.WindowManager
import android.widget.LinearLayout
import android.widget.PopupWindow
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.core.widget.PopupWindowCompat
import mozilla.components.browser.menu.R

/**
 * A tooltip shown on long click on an anchor view.
 * There can be only one tooltip shown at a given moment.
 */
internal class CustomTooltip private constructor(
    private val anchor: View,
    private val tooltipText: CharSequence,
) : OnLongClickListener, View.OnAttachStateChangeListener {
    private val hideTooltipRunnable = Runnable { hide() }
    private var popupWindow: PopupWindow? = null

    init {
        anchor.setOnLongClickListener(this)
    }

    override fun onLongClick(view: View): Boolean {
        if (ViewCompat.isAttachedToWindow(anchor)) {
            show()
            anchor.addOnAttachStateChangeListener(this)
        }
        return true
    }

    private fun computeOffsets(): Offset {
        // Measure pop-up
        val spec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
        popupWindow?.contentView?.measure(spec, spec)

        val rootView = anchor.rootView
        val rootPosition = IntArray(2)
        val anchorPosition = IntArray(2)
        rootView.getLocationOnScreen(rootPosition)
        anchor.getLocationOnScreen(anchorPosition)

        val rootY = rootPosition[1]
        val anchorY = anchorPosition[1]

        val rootHeight = rootView.height
        val tooltipHeight = popupWindow?.contentView?.measuredHeight ?: 0
        val tooltipWidth = popupWindow?.contentView?.measuredWidth ?: 0

        val checkY = rootY + rootHeight - (anchorY + anchor.height + tooltipHeight + TOOLTIP_EXTRA_VERTICAL_OFFSET_DP)
        val belowY = TOOLTIP_EXTRA_VERTICAL_OFFSET_DP
        val aboveY = -(anchor.height + tooltipHeight + TOOLTIP_EXTRA_VERTICAL_OFFSET_DP)

        // align anchor center with tooltip center
        val offsetX = anchor.width / 2 - tooltipWidth / 2
        // make sure tooltip is visible and it's not displayed below, outside the view
        val offsetY = if (checkY > 0) belowY else aboveY
        return Offset(offsetX, offsetY)
    }

    @SuppressLint("InflateParams")
    fun show() {
        activeTooltip?.hide()
        activeTooltip = this

        val layout = LayoutInflater.from(anchor.context)
            .inflate(R.layout.mozac_browser_tooltip_layout, null)

        layout.findViewById<TextView>(R.id.mozac_browser_tooltip_text).text = tooltipText
        popupWindow = PopupWindow(
            layout,
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT,
            false,
        )

        val offsets = computeOffsets()

        popupWindow?.let {
            PopupWindowCompat.setWindowLayoutType(it, WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL)
            it.isTouchable = false
            it.showAsDropDown(anchor, offsets.x, offsets.y, Gravity.CENTER)
        }

        anchor.removeCallbacks(hideTooltipRunnable)
        anchor.postDelayed(hideTooltipRunnable, LONG_CLICK_HIDE_TIMEOUT_MS)
    }

    fun hide() {
        if (activeTooltip === this) {
            activeTooltip = null
            popupWindow?.let {
                it.dismiss()
                popupWindow = null
                anchor.removeOnAttachStateChangeListener(this)
            }
        }
    }

    override fun onViewAttachedToWindow(v: View) {
        // no-op
    }

    override fun onViewDetachedFromWindow(v: View) {
        hide()
        anchor.removeCallbacks(hideTooltipRunnable)
    }

    companion object {
        private const val LONG_CLICK_HIDE_TIMEOUT_MS: Long = 2500
        private const val TOOLTIP_EXTRA_VERTICAL_OFFSET_DP = 8

        // The tooltip currently being shown properly disposed in hide() / onViewDetachedFromWindow()
        @SuppressLint("StaticFieldLeak")
        private var activeTooltip: CustomTooltip? = null

        /**
         * Set the tooltip text for the view.
         * @param view view to set the tooltip for
         * @param tooltipText the tooltip text
         */
        fun setTooltipText(view: View, tooltipText: CharSequence) {
            // check for dynamic content description
            if (TextUtils.isEmpty(tooltipText)) {
                activeTooltip?.let {
                    if (it.anchor === view) {
                        it.hide()
                    }
                }
                view.setOnLongClickListener(null)
                view.isLongClickable = false
            } else {
                CustomTooltip(view, tooltipText)
            }
        }
    }

    /**
     * A data class for storing x and y offsets
     */
    data class Offset(val x: Int, val y: Int)
}
