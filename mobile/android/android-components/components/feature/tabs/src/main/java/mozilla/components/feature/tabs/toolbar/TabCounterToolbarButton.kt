/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.toolbar

import android.view.HapticFeedbackConstants
import android.view.View
import android.view.ViewGroup
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.tabs.R
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.ui.tabcounter.TabCounter
import java.lang.ref.WeakReference

/**
 * A [Toolbar.Action] implementation that shows a [TabCounter].
 */
class TabCounterToolbarButton(
    private val sessionManager: SessionManager,
    private val showTabs: () -> Unit
) : Toolbar.Action {
    private var reference: WeakReference<TabCounter> = WeakReference<TabCounter>(null)

    override fun createView(parent: ViewGroup): View {
        sessionManager.register(sessionManagerObserver, view = parent)

        val view = TabCounter(parent.context).apply {
            reference = WeakReference(this)
            setCount(sessionManager.sessions.size)
            setOnClickListener {
                it.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
                showTabs.invoke()
            }
            contentDescription = parent.context.getString(R.string.mozac_feature_tabs_toolbar_tabs_button)
        }

        // Set selectableItemBackgroundBorderless
        val selectableItemBackgroundBorderless =
            parent.context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless)
        view.setBackgroundResource(selectableItemBackgroundBorderless)
        return view
    }

    override fun bind(view: View) = Unit

    private fun updateCount() {
        reference.get()?.setCountWithAnimation(sessionManager.sessions.size)
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
