/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.annotation.LayoutRes
import androidx.core.view.updateLayoutParams
import mozilla.components.browser.menu2.R
import mozilla.components.browser.menu2.ext.applyStyle
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate

internal class DecorativeTextMenuCandidateViewHolder(
    itemView: View,
    inflater: LayoutInflater,
) : MenuCandidateViewHolder<DecorativeTextMenuCandidate>(itemView, inflater) {

    private val textView: TextView get() = itemView as TextView

    override fun bind(
        newCandidate: DecorativeTextMenuCandidate,
        oldCandidate: DecorativeTextMenuCandidate?,
    ) {
        super.bind(newCandidate, oldCandidate)

        textView.text = newCandidate.text
        textView.applyStyle(newCandidate.textStyle, oldCandidate?.textStyle)
        applyHeight(newCandidate.height, oldCandidate?.height)
    }

    private fun applyHeight(newHeight: Int?, oldHeight: Int?) {
        if (newHeight != oldHeight) {
            textView.updateLayoutParams {
                height = newHeight ?: itemView.resources
                    .getDimensionPixelSize(R.dimen.mozac_browser_menu2_candidate_container_layout_height)
            }
        }
    }

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_candidate_decorative_text
    }
}
