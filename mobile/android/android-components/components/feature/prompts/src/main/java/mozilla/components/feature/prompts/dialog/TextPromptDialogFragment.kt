/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import mozilla.components.feature.prompts.R

private const val KEY_USER_EDIT_TEXT = "KEY_USER_EDIT_TEXT"
private const val KEY_LABEL_INPUT = "KEY_LABEL_INPUT"
private const val KEY_DEFAULT_INPUT_VALUE = "KEY_DEFAULT_INPUT_VALUE"

/**
 * [androidx.fragment.app.DialogFragment] implementation to display a
 * <a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/prompt">Window.prompt()</a> with native dialogs.
 */
internal class TextPromptDialogFragment : AbstractPromptTextDialogFragment(), TextWatcher {
    /**
     * Contains the <a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/prompt#Parameters">default()</a>
     * value provided by this [sessionId].
     */
    internal val defaultInputValue: String? by lazy { safeArguments.getString(KEY_DEFAULT_INPUT_VALUE) }

    /**
     * Contains the <a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/prompt#Parameters">message</a>
     * value provided by this [sessionId].
     */
    internal val labelInput: String? by lazy { safeArguments.getString(KEY_LABEL_INPUT) }

    private var userSelectionEditText: String
        get() = safeArguments.getString(KEY_USER_EDIT_TEXT, defaultInputValue)
        set(value) {
            safeArguments.putString(KEY_USER_EDIT_TEXT, value)
        }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = AlertDialog.Builder(requireContext())
            .setTitle(title)
            .setCancelable(true)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                onPositiveClickAction()
            }
        return addLayout(builder).create()
    }

    override fun onCancel(dialog: DialogInterface) {
        feature?.onCancel(sessionId, promptRequestUID)
    }

    private fun onPositiveClickAction() {
        feature?.onConfirm(sessionId, promptRequestUID, userSelectionNoMoreDialogs to userSelectionEditText)
    }

    @SuppressLint("InflateParams")
    private fun addLayout(builder: AlertDialog.Builder): AlertDialog.Builder {
        val inflater = LayoutInflater.from(requireContext())
        val view = inflater.inflate(R.layout.mozac_feature_text_prompt, null)

        val label = view.findViewById<TextView>(R.id.input_label)
        val editText = view.findViewById<EditText>(R.id.input_value)

        label.text = labelInput
        editText.setText(defaultInputValue)
        editText.addTextChangedListener(this)

        addCheckBoxIfNeeded(view)

        return builder.setView(view)
    }

    override fun afterTextChanged(editable: Editable) {
        userSelectionEditText = editable.toString()
    }

    override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) = Unit

    override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) = Unit

    companion object {
        /**
         * A builder method for creating a [TextPromptDialogFragment]
         * @param sessionId to create the dialog.
         * @param promptRequestUID identifier of the [PromptRequest] for which this dialog is shown.
         * @param shouldDismissOnLoad whether or not the dialog should automatically be dismissed
         * when a new page is loaded.
         * @param title the title of the dialog.
         * @param inputLabel
         * @param defaultInputValue
         * @param hasShownManyDialogs tells if this [sessionId] has shown many dialogs
         * in a short period of time, if is true a checkbox will be part of the dialog, for the user
         * to choose if wants to prevent this [sessionId] continuing showing dialogs.
         */
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            shouldDismissOnLoad: Boolean,
            title: String,
            inputLabel: String,
            defaultInputValue: String,
            hasShownManyDialogs: Boolean,
        ): TextPromptDialogFragment {
            val fragment = TextPromptDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putString(KEY_TITLE, title)
                putString(KEY_LABEL_INPUT, inputLabel)
                putString(KEY_DEFAULT_INPUT_VALUE, defaultInputValue)
                putBoolean(KEY_MANY_ALERTS, hasShownManyDialogs)
            }

            fragment.arguments = arguments
            return fragment
        }
    }
}
