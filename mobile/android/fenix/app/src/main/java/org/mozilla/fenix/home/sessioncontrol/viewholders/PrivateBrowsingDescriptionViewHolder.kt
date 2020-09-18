/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.text.method.LinkMovementMethod
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.private_browsing_description.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.addUnderline
import org.mozilla.fenix.home.sessioncontrol.TabSessionInteractor

class PrivateBrowsingDescriptionViewHolder(
    view: View,
    private val interactor: TabSessionInteractor
) : RecyclerView.ViewHolder(view) {

    init {
        val resources = view.resources
        val appName = resources.getString(R.string.app_name)
        view.private_session_description.text = resources.getString(
            R.string.private_browsing_placeholder_description_2, appName
        )
        with(view.private_session_common_myths) {
            movementMethod = LinkMovementMethod.getInstance()
            addUnderline()
            setOnClickListener {
                interactor.onPrivateBrowsingLearnMoreClicked()
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.private_browsing_description
    }
}
