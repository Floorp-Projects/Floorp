/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.annotation.LayoutRes
import androidx.constraintlayout.widget.ConstraintLayout
import mozilla.components.browser.menu2.R
import mozilla.components.browser.menu2.adapter.icons.MenuIconAdapter
import mozilla.components.browser.menu2.ext.applyBackgroundEffect
import mozilla.components.browser.menu2.ext.applyStyle
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.NestedMenuCandidate

internal class NestedMenuCandidateViewHolder(
    itemView: View,
    inflater: LayoutInflater,
    dismiss: () -> Unit,
    private val reopenMenu: (NestedMenuCandidate) -> Unit,
) : MenuCandidateViewHolder<NestedMenuCandidate>(itemView, inflater), View.OnClickListener {

    private val layout = itemView as ConstraintLayout
    private val textView: TextView get() = itemView.findViewById(R.id.label)
    private val startIcon = MenuIconAdapter(layout, inflater, Side.START, dismiss)
    private val endIcon = MenuIconAdapter(layout, inflater, Side.END, dismiss)

    init {
        itemView.setOnClickListener(this)
    }

    override fun bind(newCandidate: NestedMenuCandidate, oldCandidate: NestedMenuCandidate?) {
        super.bind(newCandidate, oldCandidate)

        textView.text = newCandidate.text
        textView.applyStyle(newCandidate.textStyle, oldCandidate?.textStyle)
        itemView.applyBackgroundEffect(newCandidate.effect, oldCandidate?.effect)
        startIcon.bind(newCandidate.start, oldCandidate?.start)
        endIcon.bind(newCandidate.end, oldCandidate?.end)
    }

    override fun onClick(v: View?) {
        lastCandidate?.let { reopenMenu(it) }
    }

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_candidate_nested
    }
}
