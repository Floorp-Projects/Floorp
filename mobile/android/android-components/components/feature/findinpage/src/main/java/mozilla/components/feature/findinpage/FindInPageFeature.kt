/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage

import android.support.annotation.CallSuper
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * This feature will subscribe to the currently selected [Session] and when started will forward
 * [Session.Observer.onFindResult] calls to the [findInPageView] ui widget.
 *
 * @property sessionManager The [SessionManager] instance in order to subscribe to the selected [Session].
 * @property findInPageView The [FindInPageView] instance that will capture user interactions, and new calls
 *  of [Session.Observer.onFindResult] will be delegated.
 */

class FindInPageFeature(
    private val sessionManager: SessionManager,
    private val findInPageView: FindInPageView
) : LifecycleAwareFeature {

    private val observer = FindInPageRequestObserver(sessionManager, feature = this)

    /**
     * Start observing the selected session and forwarding events to the [findInPageView] widget.
     */
    override fun start() {
        observer.observeSelected()
        val selectedSession = sessionManager.selectedSession ?: return
        val engineSession = sessionManager.getEngineSession(selectedSession) ?: return
        findInPageView.engineSession = engineSession
    }

    /**
     * Stop observing the selected session and forwarding events to the [findInPageView] widget.
     */
    override fun stop() {
        observer.stop()
    }

    /**
     * Informs the [findInPageView] widget that the back button has been pressed.
     *
     * @return true if the event was handled, otherwise false.
     */
    fun onBackPressed(): Boolean {
        return if (findInPageView.isActive()) {
            findInPageView.onCloseButtonClicked()
            true
        } else {
            false
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun onFindResult(session: Session, result: Session.FindResult) {
        findInPageView.onFindResultReceived(result)
    }

    internal class FindInPageRequestObserver(
        private val sessionManager: SessionManager,
        private val feature: FindInPageFeature
    ) : SelectionAwareSessionObserver(sessionManager) {

        override fun onFindResult(session: Session, result: Session.FindResult) {
            feature.onFindResult(session, result)
        }

        @CallSuper
        override fun onSessionSelected(session: Session) {
            super.onSessionSelected(session)
            val engineSession = sessionManager.getEngineSession(session) ?: return
            feature.findInPageView.engineSession = engineSession
        }
    }
}
