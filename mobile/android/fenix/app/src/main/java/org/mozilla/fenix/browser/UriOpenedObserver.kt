/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry
import org.mozilla.fenix.utils.Settings

class UriOpenedObserver(
    private val settings: Settings,
    private val owner: LifecycleOwner,
    private val sessionManager: SessionManager,
    metrics: MetricController,
    ads: AdsTelemetry
) : SessionManager.Observer {

    constructor(activity: FragmentActivity) : this(
        activity.applicationContext.settings(),
        activity,
        activity.components.core.sessionManager,
        activity.metrics,
        activity.components.core.adsTelemetry
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
        settings.setOpenTabsCount(sessionManager.sessions.filter { !it.private }.size)
        sessionManager.sessions.forEach {
            it.unregister(singleSessionObserver)
        }
    }

    override fun onSessionAdded(session: Session) {
        settings.setOpenTabsCount(sessionManager.sessions.filter { !it.private }.size)
        session.register(singleSessionObserver, owner)
    }

    override fun onSessionRemoved(session: Session) {
        settings.setOpenTabsCount(sessionManager.sessions.filter { !it.private }.size)
        session.unregister(singleSessionObserver)
    }

    override fun onSessionsRestored() {
        settings.setOpenTabsCount(sessionManager.sessions.filter { !it.private }.size)
        sessionManager.sessions.forEach {
            it.register(singleSessionObserver, owner)
        }
    }
}
