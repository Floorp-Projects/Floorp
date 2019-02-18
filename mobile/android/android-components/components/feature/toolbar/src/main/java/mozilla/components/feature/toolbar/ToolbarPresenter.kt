/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.toolbar.Toolbar

/**
 * Presenter implementation for a toolbar implementation in order to update the toolbar whenever
 * the state of the selected session changes.
 */
class ToolbarPresenter(
    private val toolbar: Toolbar,
    private val sessionManager: SessionManager,
    private val sessionId: String? = null
) : SelectionAwareSessionObserver(sessionManager) {

    /**
     * Start presenter: Display data in toolbar.
     */
    fun start() {
        val session = sessionId?.let { sessionManager.findSessionById(sessionId) }
        session?.let { observeFixed(it) } ?: observeSelected()
        initializeView()
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

        toolbar.url = session?.url ?: ""
        toolbar.displayProgress(session?.progress ?: 0)
        updateToolbarSecurity(session?.securityInfo ?: Session.SecurityInfo())
    }

    override fun onUrlChanged(session: Session, url: String) {
        toolbar.url = url
        toolbar.setSearchTerms(session.searchTerms)
    }

    override fun onProgress(session: Session, progress: Int) {
        toolbar.displayProgress(progress)
    }

    override fun onSearch(session: Session, searchTerms: String) {
        toolbar.setSearchTerms(searchTerms)
    }

    override fun onSecurityChanged(session: Session, securityInfo: Session.SecurityInfo) =
        updateToolbarSecurity(securityInfo)

    private fun updateToolbarSecurity(securityInfo: Session.SecurityInfo) =
        when (securityInfo.secure) {
            true -> toolbar.siteSecure = Toolbar.SiteSecurity.SECURE
            false -> toolbar.siteSecure = Toolbar.SiteSecurity.INSECURE
        }
}
