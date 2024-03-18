/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import android.view.View
import androidx.annotation.LayoutRes
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.candidate.DividerMenuCandidate

/**
 * View holder that displays a divider.
 */
internal class DividerMenuCandidateViewHolder(
    itemView: View,
    inflater: LayoutInflater,
) : MenuCandidateViewHolder<DividerMenuCandidate>(itemView, inflater) {

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_candidate_divider
    }
}
