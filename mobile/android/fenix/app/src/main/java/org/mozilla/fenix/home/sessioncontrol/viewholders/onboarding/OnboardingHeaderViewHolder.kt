/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.OnboardingHeaderBinding

class OnboardingHeaderViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        val binding = OnboardingHeaderBinding.bind(view)

        val appName = view.context.getString(R.string.app_name)
        binding.headerText.text = view.context.getString(R.string.onboarding_header, appName)
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_header
    }
}
