/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.internal

import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.facts.emitCloseFact
import mozilla.components.feature.findinpage.facts.emitCommitFact
import mozilla.components.feature.findinpage.facts.emitNextFact
import mozilla.components.feature.findinpage.facts.emitPreviousFact
import mozilla.components.feature.findinpage.view.FindInPageView
import mozilla.components.support.ktx.android.view.hideKeyboard

/**
 * Interactor that implements [FindInPageView.Listener] and notifies the engine or feature about actions the user
 * performed (e.g. "find next result").
 */
internal class FindInPageInteractor(
    private val feature: FindInPageFeature,
    private val view: FindInPageView,
    private val engineView: EngineView?,
) : FindInPageView.Listener {
    private var engineSession: EngineSession? = null

    fun start() {
        view.listener = this
    }

    fun stop() {
        view.listener = null
    }

    fun bind(session: SessionState) {
        engineSession = session.engineState.engineSession
    }

    override fun onPreviousResult() {
        engineSession?.findNext(forward = false)
        engineView?.asView()?.clearFocus()
        view.asView().hideKeyboard()
        emitPreviousFact()
    }

    override fun onNextResult() {
        engineSession?.findNext(forward = true)
        engineView?.asView()?.clearFocus()
        view.asView().hideKeyboard()
        emitNextFact()
    }

    override fun onClose() {
        // We pass this event up to the feature. The feature is responsible for unbinding its sub components and
        // potentially notifying other dependencies.
        feature.unbind()
        emitCloseFact()
    }

    fun unbind() {
        engineSession?.clearFindMatches()
        engineSession = null
    }

    override fun onFindAll(query: String) {
        engineSession?.findAll(query)
        emitCommitFact(query)
    }

    override fun onClearMatches() {
        engineSession?.clearFindMatches()
    }
}
