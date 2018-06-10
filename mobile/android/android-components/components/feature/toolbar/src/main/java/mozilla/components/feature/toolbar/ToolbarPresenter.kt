/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

/**
 * Presenter implementation for a toolbar implementation in order to update the toolbar whenever
 * the state of the selected session changes.
 */
class ToolbarPresenter(
    private val toolbar: Toolbar,
    private val sessionManager: SessionManager
) : SessionManager.Observer, Session.Observer {
    var session: Session = sessionManager.selectedSession

    /**
     * Start presenter: Display data in toolbar.
     */
    fun start() {
        session = sessionManager.selectedSession

        sessionManager.register(this)
        session.register(this)

        initializeView()
    }

    /**
     * Stop presenter from updating the view.
     */
    fun stop() {
        sessionManager.unregister(this)
        session.unregister(this)
    }

    /**
     * A new session has been selected: Update toolbar to display data of new session.
     */
    override fun onSessionSelected(session: Session) {
        this.session.unregister(this)

        this.session = session
        session.register(this)

        initializeView()
    }

    private fun initializeView() {
        toolbar.url = session.url
    }

    override fun onUrlChanged() {
        toolbar.url = session.url
        toolbar.setSearchTerms(session.searchTerms)
    }

    override fun onProgress() {
        toolbar.displayProgress(session.progress)
    }

    override fun onSearch() {
        toolbar.setSearchTerms(session.searchTerms)
    }

    override fun onLoadingStateChanged() { }

    override fun onNavigationStateChanged() { /* TODO */ }

    override fun onSecurityChanged() { /* TODO */ }
}
