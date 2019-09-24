/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics

class UriOpenedObserver(
    private val owner: LifecycleOwner,
    private val sessionManager: SessionManager,
    private val metrics: MetricController
) : SessionManager.Observer {

    constructor(activity: FragmentActivity) : this(
        activity,
        activity.components.core.sessionManager,
        activity.metrics
    )

    init {
        sessionManager.register(this, owner)
    }

    @VisibleForTesting
    internal val singleSessionObserver = object : Session.Observer {
        private var urlLoading: String? = null

        override fun onLoadingStateChanged(session: Session, loading: Boolean) {
            if (loading) {
                urlLoading = session.url
            } else if (urlLoading != null && !session.private) {
                metrics.track(Event.UriOpened)
            }
        }
    }

    override fun onAllSessionsRemoved() {
        sessionManager.sessions.forEach {
            it.unregister(singleSessionObserver)
        }
    }

    override fun onSessionAdded(session: Session) {
        session.register(singleSessionObserver, owner)
    }

    override fun onSessionRemoved(session: Session) {
        session.unregister(singleSessionObserver)
    }

    override fun onSessionsRestored() {
        sessionManager.sessions.forEach {
            it.register(singleSessionObserver, owner)
        }
    }
}
