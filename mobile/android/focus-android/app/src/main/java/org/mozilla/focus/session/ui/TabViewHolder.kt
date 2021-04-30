/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.view.View
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.focus.R
import org.mozilla.focus.ext.beautifyUrl
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.telemetry.TelemetryWrapper
import java.lang.ref.WeakReference

class TabViewHolder internal constructor(
    private val fragment: TabSheetFragment,
    private val textView: TextView
) : RecyclerView.ViewHolder(textView), View.OnClickListener {
    companion object {
        internal const val LAYOUT_ID = R.layout.item_session
    }

    private var tabReference: WeakReference<TabSessionState> = WeakReference<TabSessionState>(null)

    init {
        textView.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_link, 0, 0, 0)
        textView.setOnClickListener(this)
    }

    fun bind(tab: TabSessionState) {
        this.tabReference = WeakReference(tab)

        updateTitle(tab)

        val isSelected = fragment.requireComponents.store.state.selectedTabId == tab.id

        updateTextBackgroundColor(isSelected)
    }

    private fun updateTextBackgroundColor(isCurrentSession: Boolean) {
        val drawable = if (isCurrentSession) {
            R.drawable.background_list_item_current_session
        } else {
            R.drawable.background_list_item_session
        }
        textView.setBackgroundResource(drawable)
    }

    private fun updateTitle(tab: TabSessionState) {
        textView.text =
            if (tab.content.title.isEmpty()) tab.content.url.beautifyUrl()
            else tab.content.title
    }

    override fun onClick(view: View) {
        val tab = tabReference.get() ?: return
        selectSession(tab)
    }

    private fun selectSession(tab: TabSessionState) {
        fragment.animateAndDismiss().addListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                fragment.components?.tabsUseCases?.selectTab?.invoke(tab.id)

                TelemetryWrapper.switchTabInTabsTrayEvent()
            }
        })
    }
}
