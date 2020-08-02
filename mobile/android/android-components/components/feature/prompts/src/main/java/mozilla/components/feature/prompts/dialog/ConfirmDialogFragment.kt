/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.appcompat.app.AlertDialog

internal const val KEY_POSITIVE_BUTTON = "KEY_POSITIVE_BUTTON"
internal const val KEY_NEGATIVE_BUTTON = "KEY_NEGATIVE_BUTTON"

/**
 * [android.support.v4.app.DialogFragment] implementation for a confirm dialog.
 * The user has two possible options, allow the request or
 * deny it (Positive and Negative buttons]. When the positive button is pressed the
 * feature.onConfirm function will be called otherwise the feature.onCancel function will be called.
 */
internal class ConfirmDialogFragment : PromptDialogFragment() {

    private val positiveButtonText: String by lazy { safeArguments.getString(KEY_POSITIVE_BUTTON)!! }
    private val negativeButtonText: String by lazy { safeArguments.getString(KEY_NEGATIVE_BUTTON)!! }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = AlertDialog.Builder(requireContext())
            .setCancelable(false)
            .setTitle(title)
            .setMessage(message)
            .setNegativeButton(negativeButtonText) { _, _ ->
                feature?.onCancel(sessionId)
            }
            .setPositiveButton(positiveButtonText) { _, _ ->
                onPositiveClickAction()
            }
        return builder.create()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId)
    }

    private fun onPositiveClickAction() {
        feature?.onConfirm(sessionId, null)
    }

    companion object {
        fun newInstance(
            sessionId: String? = null,
            title: String,
            message: String,
            positiveButtonText: String,
            negativeButtonText: String
        ): ConfirmDialogFragment {

            val fragment = ConfirmDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_TITLE, title)
                putString(KEY_MESSAGE, message)
                putString(KEY_POSITIVE_BUTTON, positiveButtonText)
                putString(KEY_NEGATIVE_BUTTON, negativeButtonText)
            }

            fragment.arguments = arguments
            return fragment
        }
    }
}
