/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import android.view.View
import androidx.annotation.CallSuper
import mozilla.components.browser.menu2.ext.applyStyle
import mozilla.components.concept.menu.candidate.MenuCandidate

internal abstract class MenuCandidateViewHolder<T : MenuCandidate>(
    itemView: View,
    protected val inflater: LayoutInflater,
) : LastItemViewHolder<T>(itemView) {

    @CallSuper
    override fun bind(newCandidate: T, oldCandidate: T?) {
        itemView.applyStyle(newCandidate.containerStyle, oldCandidate?.containerStyle)
    }
}
