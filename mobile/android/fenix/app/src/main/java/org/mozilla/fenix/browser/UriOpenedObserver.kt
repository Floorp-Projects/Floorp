/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
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
    private val context: Context,
    private val owner: LifecycleOwner,
    private val sessionManager: SessionManager,
    private val metrics: MetricController
) : SessionManager.Observer {

    constructor(activity: FragmentActivity) : this(
        activity,
        activity,
        activity.components.core.sessionManager,
        activity.metrics
    )

    /**
     * Currently, [Session.Observer.onLoadingStateChanged] is called multiple times the first
     * time a new session loads a page. This is inflating our telemetry numbers, so we need to
     * handle it, but we will be able to remove this code when [onLoadingStateChanged] has
     * been fixed.
     *
     * See Fenix #3676
     * See AC https://github.com/mozilla-mobile/android-components/issues/4795
     * TODO remove this class after AC #4795 has been fixed
     */
    private class TemporaryFix {
        var eventSentFor: String? = null

        fun shouldSendEvent(newUrl: String): Boolean = eventSentFor != newUrl
    }

    @VisibleForTesting
    internal val singleSessionObserver = object : Session.Observer {
        private var urlLoading: String? = null

        private val temporaryFix = TemporaryFix()

        override fun onLoadingStateChanged(session: Session, loading: Boolean) {
            if (loading) {
                urlLoading = session.url
            } else if (urlLoading != null && !session.private && temporaryFix.shouldSendEvent(session.url)) {
                temporaryFix.eventSentFor = session.url
                metrics.track(Event.UriOpened)
            }
        }
    }

    init {
        sessionManager.register(this, owner)
        sessionManager.selectedSession?.register(singleSessionObserver, owner)
    }

    override fun onSessionSelected(session: Session) {
        session.register(singleSessionObserver, owner)
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
