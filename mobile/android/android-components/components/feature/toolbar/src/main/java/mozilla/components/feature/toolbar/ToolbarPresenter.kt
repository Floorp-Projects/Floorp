/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

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
) : SessionManager.Observer, Session.Observer {
    private var activeSession: Session? = null

    /**
     * Start presenter: Display data in toolbar.
     */
    fun start() {
        activeSession = sessionId?.let {
            sessionManager.findSessionById(sessionId)
        } ?: run {
            sessionManager.register(this)
            sessionManager.selectedSession
        }

        activeSession?.register(this)
        initializeView()
    }

    /**
     * Stop presenter from updating the view.
     */
    fun stop() {
        sessionManager.unregister(this)
        activeSession?.unregister(this)
    }

    /**
     * A new session has been selected: Update toolbar to display data of new session.
     */
    override fun onSessionSelected(session: Session) {
        activeSession?.unregister(this)

        activeSession = session
        session.register(this)

        initializeView()
    }

    private fun initializeView() {
        activeSession?.let { session ->
            toolbar.url = session.url

            session.customTabConfig?.toolbarColor?.let {
                toolbar.asView().setBackgroundColor(it)
            }
            // TODO Apply remaining configurations: https://github.com/mozilla-mobile/android-components/issues/306
        }
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
}
