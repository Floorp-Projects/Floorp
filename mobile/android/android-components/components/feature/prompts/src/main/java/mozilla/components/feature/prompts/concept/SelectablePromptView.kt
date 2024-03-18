/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

import android.view.View

/**
 * An interface for views that can display an option selection prompt.
 */
interface SelectablePromptView<T> {

    var listener: Listener<T>?

    /**
     * Shows an option selection prompt with the provided options.
     *
     * @param options A list of options to display in the prompt.
     */
    fun showPrompt(options: List<T>)

    /**
     * Hides the option selection prompt.
     */
    fun hidePrompt()

    /**
     * Casts this [SelectablePromptView] interface to an Android [View] object.
     */
    fun asView(): View = (this as View)

    /**
     * Interface to allow a class to listen to the option selection prompt events.
     */
    interface Listener<in T> {
        /**
         * Called when an user selects an options from the prompt.
         *
         * @param option The selected option.
         */
        fun onOptionSelect(option: T)

        /**
         * Called when the user invokes the option to manage the list of options.
         */
        fun onManageOptions()
    }
}
