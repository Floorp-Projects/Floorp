/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

/**
 * An interface for views that can display a generated strong password prompt.
 */
interface PasswordPromptView {

    var listener: Listener?

    /**
     * Shows a simple prompt with the given [generatedPassword].
     */
    fun showPrompt(
        generatedPassword: String,
        url: String,
        onSaveLoginWithStrongPassword: (url: String, password: String) -> Unit,
    )

    /**
     * Hides the prompt.
     */
    fun hidePrompt()

    /**
     * Interface to allow a class to listen to generated strong password event events.
     */
    interface Listener {
        /**
         * Called when a user wants to use a strong generated password.
         *
         */
        fun onUseGeneratedPassword(
            generatedPassword: String,
            url: String,
            onSaveLoginWithStrongPassword: (url: String, password: String) -> Unit,
        )
    }
}
