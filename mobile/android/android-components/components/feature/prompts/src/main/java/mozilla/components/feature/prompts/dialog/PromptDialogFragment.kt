/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import androidx.fragment.app.DialogFragment
import mozilla.components.concept.storage.LoginValidationDelegate
import mozilla.components.feature.prompts.login.LoginExceptions

internal const val KEY_SESSION_ID = "KEY_SESSION_ID"
internal const val KEY_TITLE = "KEY_TITLE"
internal const val KEY_MESSAGE = "KEY_MESSAGE"

/**
 * An abstract representation for all different types of prompt dialogs.
 * for handling [PromptFeature] dialogs.
 */
internal abstract class PromptDialogFragment : DialogFragment() {

    /**
     * Whether or not the dialog should automatically be dismissed when a new page is loaded.
     */
    open fun shouldDismissOnLoad(): Boolean = true

    var feature: Prompter? = null

    internal val sessionId: String by lazy { requireNotNull(arguments).getString(KEY_SESSION_ID)!! }

    internal val title: String by lazy { safeArguments.getString(KEY_TITLE)!! }

    internal val message: String by lazy { safeArguments.getString(KEY_MESSAGE)!! }

    val safeArguments get() = requireNotNull(arguments)
}

internal interface Prompter {

    /**
     * Validates whether or not a given Login may be stored.
     *
     * Logging in will not prompt a save dialog if this is left null.
     */
    val loginValidationDelegate: LoginValidationDelegate?

    /**
     * Stores whether a site should never be prompted for logins saving.
     */
    val loginExceptionStorage: LoginExceptions?

    /**
     * Invoked when a dialog is dismissed. This consumes the [PromptFeature]
     * value from the session indicated by [sessionId].
     *
     * @param sessionId this is the id of the session which requested the prompt.
     */
    fun onCancel(sessionId: String)

    /**
     * Invoked when the user confirms the action on the dialog. This consumes
     * the [PromptFeature] value from the session indicated by [sessionId].
     *
     * @param sessionId that requested to show the dialog.
     * @param value an optional value provided by the dialog as a result of confirming the action.
     */
    fun onConfirm(sessionId: String, value: Any?)

    /**
     * Invoked when the user is requesting to clear the selected value from the dialog.
     * This consumes the [PromptFeature] value from the session indicated by [sessionId].
     *
     * @param sessionId that requested to show the dialog.
     */
    fun onClear(sessionId: String)
}
