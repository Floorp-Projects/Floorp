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
private const val KEY_POSITIVE_BUTTON_TITLE = "KEY_POSITIVE_BUTTON_TITLE"
private const val KEY_NEGATIVE_BUTTON_TITLE = "KEY_NEGATIVE_BUTTON_TITLE"
private const val KEY_NEUTRAL_BUTTON_TITLE = "KEY_NEUTRAL_BUTTON_TITLE"

/**
 * [android.support.v4.app.DialogFragment] implementation to display a confirm dialog,
 *  it can have up to three buttons, they could be positive, negative or neutral.
 */
internal class MultiButtonDialogFragment : PromptDialogFragment() {

    /**
     * Tells if a checkbox should be shown for preventing this [sessionId] from showing more dialogs.
     */
    internal val hasShownManyDialogs: Boolean by lazy { safeArguments.getBoolean(KEY_MANY_ALERTS) }

    internal val positiveButtonTitle: String? by lazy { safeArguments.getString(KEY_POSITIVE_BUTTON_TITLE) }

    internal val negativeButtonTitle: String? by lazy { safeArguments.getString(KEY_NEGATIVE_BUTTON_TITLE) }

    internal val neutralButtonTitle: String? by lazy { safeArguments.getString(KEY_NEUTRAL_BUTTON_TITLE) }

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
            .setupButtons()
        return (if (hasShownManyDialogs) addCheckbox(builder) else builder)
            .create()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId)
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

    private fun AlertDialog.Builder.setupButtons(): AlertDialog.Builder {
        if (!positiveButtonTitle.isNullOrBlank()) {
            setPositiveButton(positiveButtonTitle) { _, _ ->
                feature?.onConfirm(sessionId, userSelectionNoMoreDialogs to ButtonType.POSITIVE)
            }
        }
        if (!negativeButtonTitle.isNullOrBlank()) {
            setNegativeButton(negativeButtonTitle) { _, _ ->
                feature?.onConfirm(sessionId, userSelectionNoMoreDialogs to ButtonType.NEGATIVE)
            }
        }
        if (!neutralButtonTitle.isNullOrBlank()) {
            setNeutralButton(neutralButtonTitle) { _, _ ->
                feature?.onConfirm(sessionId, userSelectionNoMoreDialogs to ButtonType.NEUTRAL)
            }
        }
        return this
    }

    companion object {
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            title: String,
            message: String,
            hasShownManyDialogs: Boolean,
            positiveButton: String = "",
            negativeButton: String = "",
            neutralButton: String = ""
        ): MultiButtonDialogFragment {

            val fragment = MultiButtonDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_TITLE, title)
                putString(KEY_MESSAGE, message)
                putBoolean(KEY_MANY_ALERTS, hasShownManyDialogs)
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
        NEUTRAL
    }
}
