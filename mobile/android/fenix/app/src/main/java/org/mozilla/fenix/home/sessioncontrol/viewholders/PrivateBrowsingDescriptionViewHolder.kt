/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.text.SpannableString
import android.text.method.LinkMovementMethod
import android.text.style.UnderlineSpan
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.private_browsing_description.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.TabSessionInteractor

class PrivateBrowsingDescriptionViewHolder(
    view: View,
    private val interactor: TabSessionInteractor
) : RecyclerView.ViewHolder(view) {

    init {
        val resources = view.context.resources
        val appName = resources.getString(R.string.app_name)
        view.private_session_description.text = resources.getString(
            R.string.private_browsing_placeholder_description, appName
        )
        val commonMythsText = view.private_session_common_myths.text.toString()
        val textWithLink = SpannableString(commonMythsText).apply {
            setSpan(UnderlineSpan(), 0, commonMythsText.length, 0)
        }
        with(view.private_session_common_myths) {
            movementMethod = LinkMovementMethod.getInstance()
            text = textWithLink
            setOnClickListener {
                interactor.onPrivateBrowsingLearnMoreClicked()
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.private_browsing_description
    }
}
