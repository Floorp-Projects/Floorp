/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.text.SpannableString
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.ForegroundColorSpan
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.private_browsing_description.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.onNext

class PrivateBrowsingDescriptionViewHolder(
    view: View,
    private val actionEmitter: Observer<SessionControlAction>
) : RecyclerView.ViewHolder(view) {

    init {
        val resources = view.context.resources
        // Format the description text to include a hyperlink
        val appName = resources.getString(R.string.app_name)
        view.private_session_description.text = resources.getString(R.string.private_browsing_explanation, appName)
        val descriptionText = String
            .format(view.private_session_description.text.toString(), System.getProperty("line.separator"))
        val linkStartIndex = descriptionText.indexOf("\n\n") + 2
        val linkAction = object : ClickableSpan() {
            override fun onClick(widget: View?) {
                actionEmitter.onNext(TabAction.PrivateBrowsingLearnMore)
            }
        }
        val textWithLink = SpannableString(descriptionText).apply {
            setSpan(linkAction, linkStartIndex, descriptionText.length, 0)

            val colorSpan = ForegroundColorSpan(view.private_session_description.currentTextColor)
            setSpan(colorSpan, linkStartIndex, descriptionText.length, 0)
        }

        view.private_session_description.movementMethod = LinkMovementMethod.getInstance()
        view.private_session_description.text = textWithLink
    }

    companion object {
        const val LAYOUT_ID = R.layout.private_browsing_description
    }
}
