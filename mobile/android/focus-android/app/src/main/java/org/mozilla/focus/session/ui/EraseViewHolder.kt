/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import androidx.appcompat.content.res.AppCompatResources
import androidx.recyclerview.widget.RecyclerView
import android.view.View
import android.widget.TextView
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.telemetry.TelemetryWrapper

class EraseViewHolder(
    private val fragment: SessionsSheetFragment,
    itemView: View
) : RecyclerView.ViewHolder(itemView), View.OnClickListener {

    init {
        val textView = itemView as TextView
        val leftDrawable = AppCompatResources.getDrawable(itemView.getContext(), R.drawable.ic_delete)
        textView.setCompoundDrawablesWithIntrinsicBounds(leftDrawable, null, null, null)
        textView.setOnClickListener(this)
    }

    override fun onClick(view: View) {
        TelemetryWrapper.eraseInTabsTrayEvent()

        fragment.animateAndDismiss().addListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                fragment.requireComponents.tabsUseCases.removeAllTabs()
            }
        })
    }

    companion object {
        const val LAYOUT_ID = R.layout.item_erase
    }
}
