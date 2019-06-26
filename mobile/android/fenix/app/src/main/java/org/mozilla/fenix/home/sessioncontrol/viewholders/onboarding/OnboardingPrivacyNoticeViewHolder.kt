/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_privacy_notice.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.SupportUtils

class OnboardingPrivacyNoticeViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        view.header_text.setOnboardingIcon(R.drawable.ic_onboarding_privacy_notice)

        val appName = view.context.getString(R.string.app_name)
        view.description_text.text = view.context.getString(R.string.onboarding_privacy_notice_description, appName)

        view.read_button.setOnClickListener {
            val intent = SupportUtils.createCustomTabIntent(view.context, SupportUtils.PRIVACY_NOTICE_URL)
            view.context.startActivity(intent)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_privacy_notice
    }
}
