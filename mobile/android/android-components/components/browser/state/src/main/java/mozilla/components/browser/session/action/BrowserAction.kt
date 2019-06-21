/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.action

import mozilla.components.browser.session.state.SessionState
import mozilla.components.browser.session.state.BrowserState
import mozilla.components.concept.engine.HitResult
import mozilla.components.lib.state.Action

/**
 * [Action] implementation related to [BrowserState].
 */
sealed class BrowserAction : Action

/**
 * [BrowserAction] implementations related to updating the list of [SessionState] inside [BrowserState].
 */
sealed class SessionListAction : BrowserAction() {
    /**
     * Adds a new [SessionState] to the list.
     */
    data class AddSessionAction(val session: SessionState, val select: Boolean = false) : SessionListAction()

    /**
     * Marks the [SessionState] with the given [sessionId] as selected session.
     */
    data class SelectSessionAction(val sessionId: String) : SessionListAction()

    /**
     * Removes the [SessionState] with the given [sessionId] from the list of sessions.
     */
    data class RemoveSessionAction(val sessionId: String) : SessionListAction()
}

/**
 * [BrowserAction] implementations related to updating the a single [SessionState] inside [BrowserState].
 */
sealed class SessionAction : BrowserAction() {
    /**
     * Updates the URL of the [SessionState] with the given [sessionId].
     */
    data class UpdateUrlAction(val sessionId: String, val url: String) : SessionAction()

    /**
     * Updates the progress of the [SessionState] with the given [sessionId].
     */
    data class UpdateProgressAction(val sessionId: String, val progress: Int) : SessionAction()

    /**
     * Updates the "canGoBack" state of the [SessionState] with the given [sessionId].
     */
    data class CanGoBackAction(val sessionId: String, val canGoBack: Boolean) : SessionAction()

    /**
     * Updates the "canGoForward" state of the [SessionState] with the given [sessionId].
     */
    data class CanGoForward(val sessionId: String, val canGoForward: Boolean) : SessionAction()

    /**
     * Sets the [HitResult] state of the [SessionState] with the given [sessionId].
     */
    data class AddHitResultAction(val sessionId: String, val hitResult: HitResult) : SessionAction()

    /**
     * Clears the [HitResult] state of the [SessionState] with the given [sessionId].
     */
    data class ConsumeHitResultAction(val sessionId: String) : SessionAction()
}
