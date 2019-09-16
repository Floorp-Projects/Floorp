/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.widget.CheckBox
import androidx.appcompat.app.AlertDialog

private const val KEY_MANY_ALERTS = "KEY_MANY_ALERTS"
private const val KEY_USER_CHECK_BOX = "KEY_USER_CHECK_BOX"

/**
 * [android.support.v4.app.DialogFragment] implementation to display web Alerts with native dialogs.
 */
internal class AlertDialogFragment : PromptDialogFragment() {

    /**
     * Tells if a checkbox should be shown for preventing this [sessionId] from showing more dialogs.
     */
    internal val hasShownManyDialogs: Boolean by lazy { safeArguments.getBoolean(KEY_MANY_ALERTS) }

    /**
     * Stores the user's decision from the checkbox
     * for preventing this [sessionId] from showing more dialogs.
     */
    private var userSelectionNoMoreDialogs: Boolean
        get() = safeArguments.getBoolean(KEY_USER_CHECK_BOX)
        set(value) {
            safeArguments.putBoolean(KEY_USER_CHECK_BOX, value)
        }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = AlertDialog.Builder(requireContext())
            .setTitle(title)
            .setCancelable(true)
            .setMessage(message)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                onPositiveClickAction()
            }
        return (if (hasShownManyDialogs) addCheckbox(builder) else builder)
            .create()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId)
    }

    private fun onPositiveClickAction() {
        if (!userSelectionNoMoreDialogs) {
            feature?.onCancel(sessionId)
        } else {
            feature?.onConfirm(sessionId, userSelectionNoMoreDialogs)
        }
    }

    @SuppressLint("InflateParams")
    private fun addCheckbox(builder: AlertDialog.Builder): AlertDialog.Builder {
        val inflater = LayoutInflater.from(requireContext())
        val view = inflater.inflate(R.layout.mozac_feature_many_dialogs_checkbox_dialogs, null)
        val checkBox = view.findViewById<CheckBox>(R.id.no_more_dialogs_check_box)
        checkBox.setOnCheckedChangeListener { _, isChecked ->
            userSelectionNoMoreDialogs = isChecked
        }
        builder.setView(view)

        return builder
    }

    companion object {
        /**
         * A builder method for creating a [AlertDialogFragment]
         * @param sessionId to create the dialog.
         * @param title the title of the dialog.
         * @param message the message of the dialog.
         * @param hasShownManyDialogs tells if this [sessionId] has shown many dialogs
         * in a short period of time, if is true a checkbox will be part of the dialog, for the user
         * to choose if wants to prevent this [sessionId] continuing showing dialogs.
         */
        fun newInstance(
            sessionId: String,
            title: String,
            message: String,
            hasShownManyDialogs: Boolean
        ): AlertDialogFragment {

            val fragment = AlertDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_TITLE, title)
                putString(KEY_MESSAGE, message)
                putBoolean(KEY_MANY_ALERTS, hasShownManyDialogs)
            }

            fragment.arguments = arguments
            return fragment
        }
    }
}
