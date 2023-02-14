/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.OnboardingSectionHeaderBinding

class OnboardingSectionHeaderViewHolder(view: View) : RecyclerView.ViewHolder(view) {
    private val binding = OnboardingSectionHeaderBinding.bind(view)
    private val sectionHeader = binding.sectionHeaderText

    fun bind(labelBuilder: (Context) -> String) {
        sectionHeader.text = labelBuilder(itemView.context)
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_section_header
    }
}
