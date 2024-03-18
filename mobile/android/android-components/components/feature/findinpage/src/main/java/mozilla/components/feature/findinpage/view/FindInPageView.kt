/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.view

import android.view.View
import mozilla.components.browser.state.state.content.FindResultState

/**
 * An interface for views that can display "find in page" results and related UI controls.
 */
interface FindInPageView {
    /**
     * Listener to be invoked after the user performs certain actions (e.g. "find next result").
     */
    var listener: Listener?

    /**
     * Sets/gets private mode.
     *
     * In private mode the IME should not update any personalized data such as typing history and personalized language
     * model based on what the user typed.
     */
    var private: Boolean

    /**
     * Displays the given [FindResultState] state in the view.
     */
    fun displayResult(result: FindResultState)

    /**
     * Requests focus for the input element the user can type their query into.
     */
    fun focus()

    /**
     * Clears the UI state.
     */
    fun clear()

    /**
     * Casts this [FindInPageView] interface to an actual Android [View] object.
     */
    fun asView(): View = (this as View)

    interface Listener {
        fun onPreviousResult()
        fun onNextResult()
        fun onClose()
        fun onFindAll(query: String)
        fun onClearMatches()
    }
}
