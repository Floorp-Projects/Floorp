/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import android.view.View
import android.widget.CompoundButton
import androidx.annotation.LayoutRes
import androidx.constraintlayout.widget.ConstraintLayout
import mozilla.components.browser.menu2.R
import mozilla.components.browser.menu2.adapter.icons.MenuIconAdapter
import mozilla.components.browser.menu2.ext.applyBackgroundEffect
import mozilla.components.browser.menu2.ext.applyStyle
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate.ButtonType

internal abstract class CompoundMenuCandidateViewHolder(
    itemView: View,
    inflater: LayoutInflater,
    private val dismiss: () -> Unit,
) : MenuCandidateViewHolder<CompoundMenuCandidate>(itemView, inflater), CompoundButton.OnCheckedChangeListener {

    private val layout = itemView as ConstraintLayout
    private val compoundButton: CompoundButton = itemView.findViewById(R.id.label)
    private val startIcon = MenuIconAdapter(layout, inflater, Side.START, dismiss)
    private var onCheckedChangeListener: ((Boolean) -> Unit)? = null

    override fun bind(newCandidate: CompoundMenuCandidate, oldCandidate: CompoundMenuCandidate?) {
        super.bind(newCandidate, oldCandidate)
        onCheckedChangeListener = newCandidate.onCheckedChange
        compoundButton.text = newCandidate.text
        startIcon.bind(newCandidate.start, oldCandidate?.start)
        compoundButton.applyStyle(newCandidate.textStyle, oldCandidate?.textStyle)
        itemView.applyBackgroundEffect(newCandidate.effect, oldCandidate?.effect)

        // isChecked calls the listener automatically
        compoundButton.setOnCheckedChangeListener(null)
        compoundButton.isChecked = newCandidate.isChecked
        compoundButton.setOnCheckedChangeListener(this)
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        onCheckedChangeListener?.invoke(isChecked)
        dismiss()
    }

    companion object {
        @LayoutRes
        fun getLayoutResource(candidate: CompoundMenuCandidate) = when (candidate.end) {
            ButtonType.CHECKBOX -> CompoundCheckboxMenuCandidateViewHolder.layoutResource
            ButtonType.SWITCH -> CompoundSwitchMenuCandidateViewHolder.layoutResource
        }
    }
}

internal class CompoundCheckboxMenuCandidateViewHolder(
    itemView: View,
    inflater: LayoutInflater,
    dismiss: () -> Unit,
) : CompoundMenuCandidateViewHolder(itemView, inflater, dismiss) {

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_candidate_compound_checkbox
    }
}

internal class CompoundSwitchMenuCandidateViewHolder(
    itemView: View,
    inflater: LayoutInflater,
    dismiss: () -> Unit,
) : CompoundMenuCandidateViewHolder(itemView, inflater, dismiss) {

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_candidate_compound_switch
    }
}
