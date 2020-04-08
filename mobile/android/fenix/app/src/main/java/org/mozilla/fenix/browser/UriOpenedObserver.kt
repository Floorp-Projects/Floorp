/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.fenix.ads.AdsTelemetry
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics

class UriOpenedObserver(
    private val owner: LifecycleOwner,
    private val sessionManager: SessionManager,
    metrics: MetricController,
    ads: AdsTelemetry
) : SessionManager.Observer {

    constructor(activity: FragmentActivity) : this(
        activity,
        activity.components.core.sessionManager,
        activity.metrics,
        activity.components.core.ads
    )

    @VisibleForTesting
    internal val singleSessionObserver = TelemetrySessionObserver(metrics, ads)

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
