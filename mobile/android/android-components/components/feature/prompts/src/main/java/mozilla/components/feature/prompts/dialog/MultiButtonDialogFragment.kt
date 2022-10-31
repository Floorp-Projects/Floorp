/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.appcompat.app.AlertDialog

private const val KEY_POSITIVE_BUTTON_TITLE = "KEY_POSITIVE_BUTTON_TITLE"
private const val KEY_NEGATIVE_BUTTON_TITLE = "KEY_NEGATIVE_BUTTON_TITLE"
private const val KEY_NEUTRAL_BUTTON_TITLE = "KEY_NEUTRAL_BUTTON_TITLE"

/**
 * [android.support.v4.app.DialogFragment] implementation to display a confirm dialog,
 *  it can have up to three buttons, they could be positive, negative or neutral.
 */
internal class MultiButtonDialogFragment : AbstractPromptTextDialogFragment() {

    internal val positiveButtonTitle: String? by lazy { safeArguments.getString(KEY_POSITIVE_BUTTON_TITLE) }

    internal val negativeButtonTitle: String? by lazy { safeArguments.getString(KEY_NEGATIVE_BUTTON_TITLE) }

    internal val neutralButtonTitle: String? by lazy { safeArguments.getString(KEY_NEUTRAL_BUTTON_TITLE) }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = AlertDialog.Builder(requireContext())
            .setTitle(title)
            .setCancelable(true)
            .setupButtons()
        return setCustomMessageView(builder)
            .create()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId, promptRequestUID)
    }

    private fun AlertDialog.Builder.setupButtons(): AlertDialog.Builder {
        if (!positiveButtonTitle.isNullOrBlank()) {
            setPositiveButton(positiveButtonTitle) { _, _ ->
                feature?.onConfirm(sessionId, promptRequestUID, userSelectionNoMoreDialogs to ButtonType.POSITIVE)
            }
        }
        if (!negativeButtonTitle.isNullOrBlank()) {
            setNegativeButton(negativeButtonTitle) { _, _ ->
                feature?.onConfirm(sessionId, promptRequestUID, userSelectionNoMoreDialogs to ButtonType.NEGATIVE)
            }
        }
        if (!neutralButtonTitle.isNullOrBlank()) {
            setNeutralButton(neutralButtonTitle) { _, _ ->
                feature?.onConfirm(sessionId, promptRequestUID, userSelectionNoMoreDialogs to ButtonType.NEUTRAL)
            }
        }
        return this
    }

    companion object {
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            title: String,
            message: String,
            hasShownManyDialogs: Boolean,
            shouldDismissOnLoad: Boolean,
            positiveButton: String = "",
            negativeButton: String = "",
            neutralButton: String = "",
        ): MultiButtonDialogFragment {
            val fragment = MultiButtonDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putString(KEY_TITLE, title)
                putString(KEY_MESSAGE, message)
                putBoolean(KEY_MANY_ALERTS, hasShownManyDialogs)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putString(KEY_POSITIVE_BUTTON_TITLE, positiveButton)
                putString(KEY_NEGATIVE_BUTTON_TITLE, negativeButton)
                putString(KEY_NEUTRAL_BUTTON_TITLE, neutralButton)
            }
            fragment.arguments = arguments
            return fragment
        }
    }

    enum class ButtonType {
        POSITIVE,
        NEGATIVE,
        NEUTRAL,
    }
}
