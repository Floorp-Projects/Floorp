/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.*
import org.mozilla.fenix.R

class OnboardingPrivacyNoticeViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        val appName = view.context.getString(R.string.app_name)
        view.description_text.text = view.context.getString(R.string.onboarding_privacy_notice_description, appName)
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_privacy_notice
    }
}
