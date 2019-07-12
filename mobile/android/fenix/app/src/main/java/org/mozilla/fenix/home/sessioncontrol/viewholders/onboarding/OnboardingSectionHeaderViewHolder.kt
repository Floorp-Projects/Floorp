/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_section_header.view.*
import org.mozilla.fenix.R

class OnboardingSectionHeaderViewHolder(private val view: View) : RecyclerView.ViewHolder(view) {

    fun bind(labelBuilder: (Context) -> String) {
        view.section_header_text.text = labelBuilder(view.context)
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_section_header
    }
}
