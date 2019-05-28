/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import androidx.recyclerview.widget.RecyclerView
import android.view.View
import android.widget.TextView
import mozilla.components.browser.session.Session
import org.mozilla.focus.R
import org.mozilla.focus.ext.beautifyUrl
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.telemetry.TelemetryWrapper
import java.lang.ref.WeakReference

class SessionViewHolder internal constructor(
    private val fragment: SessionsSheetFragment,
    private val textView: TextView
) : RecyclerView.ViewHolder(textView), View.OnClickListener {
    companion object {
        @JvmField
        internal val LAYOUT_ID = R.layout.item_session
    }

    private var sessionReference: WeakReference<Session> = WeakReference<Session>(null)

    init {
        textView.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_link, 0, 0, 0)
        textView.setOnClickListener(this)
    }

    fun bind(session: Session) {
        this.sessionReference = WeakReference(session)

        updateTitle(session)

        val isCurrentSession = fragment.requireComponents.sessionManager.selectedSession == session

        updateTextBackgroundColor(isCurrentSession)
    }

    private fun updateTextBackgroundColor(isCurrentSession: Boolean) {
        val drawable = if (isCurrentSession) {
            R.drawable.background_list_item_current_session
        } else {
            R.drawable.background_list_item_session
        }
        textView.setBackgroundResource(drawable)
    }

    private fun updateTitle(session: Session) {
        textView.text =
            if (session.title.isEmpty()) session.url.beautifyUrl()
            else session.title
    }

    override fun onClick(view: View) {
        val session = sessionReference.get() ?: return
        selectSession(session)
    }

    private fun selectSession(session: Session) {
        fragment.animateAndDismiss().addListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                fragment.requireComponents.sessionManager.select(session)

                TelemetryWrapper.switchTabInTabsTrayEvent()
            }
        })
    }
}
