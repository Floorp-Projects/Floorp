/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.support.v4.content.ContextCompat
import android.support.v4.graphics.drawable.DrawableCompat
import android.support.v7.content.res.AppCompatResources
import android.support.v7.widget.RecyclerView
import android.view.View
import android.widget.TextView
import org.mozilla.focus.R
import org.mozilla.focus.session.Session
import org.mozilla.focus.session.SessionManager
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.ext.beautifyUrl
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
        textView.setOnClickListener(this)
    }

    fun bind(session: Session) {
        this.sessionReference = WeakReference(session)

        updateUrl(session)

        val isCurrentSession = SessionManager.getInstance().isCurrentSession(session)
        val actionColor = ContextCompat.getColor(textView.context, R.color.colorAction)
        val darkColor = ContextCompat.getColor(textView.context, R.color.colorSession)

        updateTextColor(isCurrentSession, actionColor, darkColor)
        updateDrawable(isCurrentSession, actionColor, darkColor)
    }

    private fun updateTextColor(isCurrentSession: Boolean, actionColor: Int, darkColor: Int) {
        textView.setTextColor(if (isCurrentSession) actionColor else darkColor)
    }

    private fun updateDrawable(isCurrentSession: Boolean, actionColor: Int, darkColor: Int) {
        val drawable = AppCompatResources.getDrawable(itemView.context, R.drawable.ic_link) ?: return

        val wrapDrawable = DrawableCompat.wrap(drawable.mutate())
        DrawableCompat.setTint(wrapDrawable, if (isCurrentSession) actionColor else darkColor)

        textView.setCompoundDrawablesWithIntrinsicBounds(wrapDrawable, null, null, null)
    }

    private fun updateUrl(session: Session) {
        textView.text = session.url.value.beautifyUrl()
    }

    override fun onClick(view: View) {
        val session = sessionReference.get() ?: return
        selectSession(session)
    }

    private fun selectSession(session: Session) {
        fragment.animateAndDismiss().addListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                SessionManager.getInstance().selectSession(session)

                TelemetryWrapper.switchTabInTabsTrayEvent()
            }
        })
    }
}
