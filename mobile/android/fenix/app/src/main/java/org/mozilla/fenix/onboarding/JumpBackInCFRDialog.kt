/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.databinding.OnboardingJumpBackInCfrBinding
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.recentsyncedtabs.view.RecentSyncedTabViewHolder
import org.mozilla.fenix.home.recenttabs.view.RecentTabsHeaderViewHolder

/**
 * Dialog displayed once when the jump back in section is shown in the home screen.
 */
class JumpBackInCFRDialog(val recyclerView: RecyclerView) {

    /**
     * Try to show the crf dialog if it hasn't been shown before.
     */
    fun showIfNeeded() {
        val jumpBackInView = findJumpBackInView()
        jumpBackInView?.let {
            val crfDialog = createJumpCRF(anchor = jumpBackInView)
            crfDialog?.let {
                val context = jumpBackInView.context
                context.settings().shouldShowJumpBackInCFR = false
                it.show()
            }
        }
    }

    private fun findJumpBackInView(): View? {
        val count = recyclerView.adapter?.itemCount ?: return null

        for (index in 0..count) {
            val viewHolder = recyclerView.findViewHolderForAdapterPosition(index)
            if (viewHolder is RecentTabsHeaderViewHolder) {
                return viewHolder.composeView
            }
        }
        return null
    }

    private fun hasSyncTabsView(): Boolean {
        val count = recyclerView.adapter?.itemCount ?: return false

        for (index in count downTo 0) {
            val viewHolder = recyclerView.findViewHolderForAdapterPosition(index)
            if (viewHolder is RecentSyncedTabViewHolder) {
                return true
            }
        }

        return false
    }

    private fun createJumpCRF(anchor: View): Dialog? {
        val context: Context = recyclerView.context

        if (context.settings().showSyncCFR && hasSyncTabsView()) {
            context.settings().shouldShowJumpBackInCFR = false
        }

        if (!context.settings().shouldShowJumpBackInCFR) {
            return null
        }

        val anchorPosition = IntArray(2)
        val popupBinding = OnboardingJumpBackInCfrBinding.inflate(LayoutInflater.from(context))
        val popup = Dialog(context)

        popup.apply {
            setContentView(popupBinding.root)
            setCanceledOnTouchOutside(true)
            // removing title or setting it as an empty string does not prevent a11y services from assigning one
            setTitle(" ")
        }
        popupBinding.closeInfoBanner.setOnClickListener {
            popup.dismiss()
        }

        anchor.getLocationOnScreen(anchorPosition)
        val (x, y) = anchorPosition

        if (x == 0 && y == 0) {
            return null
        }

        popupBinding.root.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED)

        popup.window?.apply {
            val attr = attributes
            setGravity(Gravity.START or Gravity.TOP)
            attr.x = x
            attr.y = y - popupBinding.root.measuredHeight
            attributes = attr
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        }
        return popup
    }
}
