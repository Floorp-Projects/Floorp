/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.focus.R
import org.mozilla.focus.databinding.ItemSessionBinding
import org.mozilla.focus.ext.beautifyUrl
import java.lang.ref.WeakReference

class TabViewHolder(
    private val binding: ItemSessionBinding,
) : RecyclerView.ViewHolder(binding.root) {

    private var tabReference: WeakReference<TabSessionState> = WeakReference<TabSessionState>(null)

    fun bind(
        tab: TabSessionState,
        isCurrentSession: Boolean,
        selectSession: (TabSessionState) -> Unit,
        closeSession: (TabSessionState) -> Unit,
    ) {
        tabReference = WeakReference(tab)

        val drawable = if (isCurrentSession) {
            R.drawable.background_list_item_current_session
        } else {
            R.drawable.background_list_item_session
        }

        val title = tab.content.title.ifEmpty { tab.content.url.beautifyUrl() }

        binding.sessionItem.setBackgroundResource(drawable)
        binding.sessionTitle.apply {
            setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_link, 0, 0, 0)
            text = title
            setOnClickListener {
                val clickedTab = tabReference.get() ?: return@setOnClickListener
                selectSession(clickedTab)
            }
        }

        binding.closeButton.setOnClickListener {
            val clickedTab = tabReference.get() ?: return@setOnClickListener
            closeSession(clickedTab)
        }
    }
}
