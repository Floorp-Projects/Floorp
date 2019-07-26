/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.internal.URLRenderer
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection

/**
 * Presenter implementation for a toolbar implementation in order to update the toolbar whenever
 * the state of the selected session changes.
 */
@Suppress("TooManyFunctions")
class ToolbarPresenter(
    private val toolbar: Toolbar,
    private val sessionManager: SessionManager,
    private val sessionId: String? = null,
    urlRenderConfiguration: ToolbarFeature.UrlRenderConfiguration? = null
) : SelectionAwareSessionObserver(sessionManager) {

    @VisibleForTesting
    internal var renderer = URLRenderer(toolbar, urlRenderConfiguration)

    /**
     * Start presenter: Display data in toolbar.
     */
    fun start() {
        observeIdOrSelected(sessionId)
        initializeView()

        renderer.start()
    }

    override fun stop() {
        super.stop()

        renderer.stop()
    }

    /**
     * A new session has been selected: Update toolbar to display data of new session.
     */
    override fun onSessionSelected(session: Session) {
        super.onSessionSelected(session)
        initializeView()
    }

    override fun onAllSessionsRemoved() {
        initializeView()
    }

    override fun onSessionRemoved(session: Session) {
        if (sessionManager.selectedSession == null) {
            initializeView()
        }
    }

    internal fun initializeView() {
        val session = sessionId?.let { sessionManager.findSessionById(sessionId) }
            ?: sessionManager.selectedSession

        renderer.post(session?.url ?: "")

        toolbar.setSearchTerms(session?.searchTerms ?: "")
        toolbar.displayProgress(session?.progress ?: 0)
        updateToolbarSecurity(session?.securityInfo ?: Session.SecurityInfo())

        if (session != null) {
            updateToolbarTrackingProtection(session)
        } else {
            toolbar.siteTrackingProtection = SiteTrackingProtection.OFF_GLOBALLY
        }
    }

    override fun onUrlChanged(session: Session, url: String) {
        renderer.post(url)
        updateToolbarTrackingProtection(session)
    }

    override fun onProgress(session: Session, progress: Int) {
        toolbar.displayProgress(progress)
    }

    override fun onSearch(session: Session, searchTerms: String) {
        toolbar.setSearchTerms(searchTerms)
    }

    override fun onSecurityChanged(session: Session, securityInfo: Session.SecurityInfo) =
        updateToolbarSecurity(securityInfo)

    override fun onTrackerBlockingEnabledChanged(session: Session, blockingEnabled: Boolean) {
        updateToolbarTrackingProtection(session)
    }

    override fun onTrackerBlocked(session: Session, tracker: Tracker, all: List<Tracker>) {
        updateToolbarTrackingProtection(session)
    }

    private fun updateToolbarSecurity(securityInfo: Session.SecurityInfo) =
        when (securityInfo.secure) {
            true -> toolbar.siteSecure = Toolbar.SiteSecurity.SECURE
            false -> toolbar.siteSecure = Toolbar.SiteSecurity.INSECURE
        }

    private fun updateToolbarTrackingProtection(session: Session) {
        val areTrackersBlocked = session.trackersBlocked.isNotEmpty()
        val siteTrackingProtection = when (session.trackerBlockingEnabled) {
            true -> {
                if (areTrackersBlocked) {
                    SiteTrackingProtection.ON_TRACKERS_BLOCKED
                } else {
                    SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED
                }
            }
            false -> SiteTrackingProtection.OFF_GLOBALLY
        }
        toolbar.siteTrackingProtection = siteTrackingProtection
    }
}
