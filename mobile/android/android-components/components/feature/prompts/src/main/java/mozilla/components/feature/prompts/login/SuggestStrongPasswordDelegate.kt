/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import mozilla.components.feature.prompts.concept.PasswordPromptView

/**
 * Delegate to display the suggest strong password prompt.
 */
interface SuggestStrongPasswordDelegate {

    /**
     * The [PasswordPromptView] used for [StrongPasswordPromptViewListener] to display a simple prompt.
     */
    val strongPasswordPromptViewListenerView: PasswordPromptView?
        get() = null
}
