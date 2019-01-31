/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.internal

import mozilla.components.browser.session.Session
import mozilla.components.feature.findinpage.view.FindInPageView

/**
 * Presenter implementation that will subscribe to a [Session] and update the view whenever the [Session.FindResult]
 * changes.
 */
internal class FindInPagePresenter(
    private val view: FindInPageView
) : Session.Observer {
    private var started: Boolean = false
    private var session: Session? = null

    fun start() {
        session?.register(this, view = view.asView())
        started = true
    }

    fun stop() {
        session?.unregister(this)
        started = false
    }

    fun bind(session: Session) {
        this.session?.unregister(this)
        this.session = session

        if (started) {
            // Only register now if we are started already. Otherwise let start() handle that.
            session.register(this, view = view.asView())
        }

        view.focus()
    }

    override fun onFindResult(session: Session, result: Session.FindResult) {
        view.displayResult(result)
    }

    fun unbind() {
        view.clear()

        session?.unregister(this)
        session = null
    }
}
