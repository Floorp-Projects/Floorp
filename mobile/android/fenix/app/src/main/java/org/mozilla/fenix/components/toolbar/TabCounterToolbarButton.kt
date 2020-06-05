/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.util.TypedValue
import android.view.View
import android.view.ViewGroup
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.toolbar.Toolbar
import org.mozilla.fenix.ext.sessionsOfType
import java.lang.ref.WeakReference

/**
 * A [Toolbar.Action] implementation that shows a [TabCounter].
 */
class TabCounterToolbarButton(
    private val sessionManager: SessionManager,
    private val isPrivate: Boolean,
    private val showTabs: () -> Unit
) : Toolbar.Action {
    private var reference: WeakReference<TabCounter> = WeakReference<TabCounter>(null)

    override fun createView(parent: ViewGroup): View {
        sessionManager.register(sessionManagerObserver, view = parent)

        val view = TabCounter(parent.context).apply {
            reference = WeakReference(this)
            setOnClickListener {
                showTabs.invoke()
            }

            addOnAttachStateChangeListener(object : View.OnAttachStateChangeListener {
                override fun onViewAttachedToWindow(v: View?) {
                    setCount(sessionManager.sessionsOfType(private = isPrivate).count())
                }

                override fun onViewDetachedFromWindow(v: View?) { /* no-op */ }
            })
        }

        // Set selectableItemBackgroundBorderless
        val outValue = TypedValue()
        parent.context.theme.resolveAttribute(
            android.R.attr.selectableItemBackgroundBorderless,
            outValue,
            true
        )
        view.setBackgroundResource(outValue.resourceId)
        return view
    }

    override fun bind(view: View) = Unit

    private fun updateCount() {
        val count = sessionManager.sessionsOfType(private = isPrivate).count()

        reference.get()?.let {
            it.setCountWithAnimation(count)
        }
    }

    private val sessionManagerObserver = object : SessionManager.Observer {
        override fun onSessionAdded(session: Session) {
            updateCount()
        }

        override fun onSessionRemoved(session: Session) {
            updateCount()
        }

        override fun onSessionsRestored() {
            updateCount()
        }

        override fun onAllSessionsRemoved() {
            updateCount()
        }
    }
}
