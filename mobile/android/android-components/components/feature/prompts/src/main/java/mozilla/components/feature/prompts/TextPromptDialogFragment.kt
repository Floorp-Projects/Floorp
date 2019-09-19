/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.View.VISIBLE
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AlertDialog

private const val KEY_MANY_ALERTS = "KEY_MANY_ALERTS"
private const val KEY_USER_CHECK_BOX = "KEY_USER_CHECK_BOX"
private const val KEY_USER_EDIT_TEXT = "KEY_USER_EDIT_TEXT"
private const val KEY_LABEL_INPUT = "KEY_LABEL_INPUT"
private const val KEY_DEFAULT_INPUT_VALUE = "KEY_DEFAULT_INPUT_VALUE"

/**
 * [android.support.v4.app.DialogFragment] implementation to display a
 * <a href="https://developer.mozilla.org/en-US/docs/Web/API/Window/prompt">Window.prompt()</a> with native dialogs.
 */
internal class TextPromptDialogFragment : PromptDialogFragment(), TextWatcher {

    /**
     * Tells if a checkbox should be shown for preventing this [sessionId] from showing more dialogs.
     */
    internal val hasShownManyDialogs: Boolean by lazy { safeArguments.getBoolean(KEY_MANY_ALERTS) }

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

    /**
     * Stores the user's decision from the checkbox
     * for preventing this [sessionId] from showing more dialogs.
     */
    internal var userSelectionNoMoreDialogs: Boolean
        get() = safeArguments.getBoolean(KEY_USER_CHECK_BOX)
        set(value) {
            safeArguments.putBoolean(KEY_USER_CHECK_BOX, value)
        }

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
        feature?.onCancel(sessionId)
    }

    private fun onPositiveClickAction() {
        feature?.onConfirm(sessionId, userSelectionNoMoreDialogs to userSelectionEditText)
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

        if (hasShownManyDialogs) {
            addCheckBox(view)
        }

        return builder.setView(view)
    }

    @SuppressLint("InflateParams")
    private fun addCheckBox(view: View) {
        if (hasShownManyDialogs) {
            val checkBox = view.findViewById<CheckBox>(R.id.no_more_dialogs_check_box)
            checkBox.setOnCheckedChangeListener { _, isChecked ->
                userSelectionNoMoreDialogs = isChecked
            }
            checkBox.visibility = VISIBLE
        }
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
         * @param title the title of the dialog.
         * @param inputLabel
         * @param defaultInputValue
         * @param hasShownManyDialogs tells if this [sessionId] has shown many dialogs
         * in a short period of time, if is true a checkbox will be part of the dialog, for the user
         * to choose if wants to prevent this [sessionId] continuing showing dialogs.
         */
        fun newInstance(
            sessionId: String,
            title: String,
            inputLabel: String,
            defaultInputValue: String,
            hasShownManyDialogs: Boolean
        ): TextPromptDialogFragment {

            val fragment = TextPromptDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
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
