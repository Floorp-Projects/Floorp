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
import android.view.View.GONE
import android.widget.EditText
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.appcompat.app.AlertDialog

private const val KEY_USERNAME_EDIT_TEXT = "KEY_USERNAME_EDIT_TEXT"
private const val KEY_PASSWORD_EDIT_TEXT = "KEY_PASSWORD_EDIT_TEXT"
private const val KEY_ONLY_SHOW_PASSWORD = "KEY_ONLY_SHOW_PASSWORD"

/**
 * [android.support.v4.app.DialogFragment] implementation to display a
 * <a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication">authentication</a>
 * dialog with native dialogs.
 */
internal class AuthenticationDialogFragment : PromptDialogFragment() {

    internal val onlyShowPassword: Boolean by lazy { safeArguments.getBoolean(KEY_ONLY_SHOW_PASSWORD) }

    internal var username: String
        get() = safeArguments.getString(KEY_USERNAME_EDIT_TEXT, "")
        set(value) {
            safeArguments.putString(KEY_USERNAME_EDIT_TEXT, value)
        }

    internal var password: String
        get() = safeArguments.getString(KEY_PASSWORD_EDIT_TEXT, "")
        set(value) {
            safeArguments.putString(KEY_PASSWORD_EDIT_TEXT, value)
        }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = AlertDialog.Builder(requireContext())
            .setupTitle()
            .setMessage(message)
            .setCancelable(true)
            .setNegativeButton(R.string.mozac_feature_prompts_cancel) { _, _ ->
                feature?.onCancel(sessionId)
            }
            .setPositiveButton(android.R.string.ok) { _, _ ->
                onPositiveClickAction()
            }
        return addLayout(builder).create()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId)
    }

    private fun onPositiveClickAction() {
        feature?.onConfirm(sessionId, username to password)
    }

    @SuppressLint("InflateParams")
    private fun addLayout(builder: AlertDialog.Builder): AlertDialog.Builder {
        val inflater = LayoutInflater.from(requireContext())
        val view = inflater.inflate(R.layout.mozac_feature_prompt_auth_prompt, null)

        bindUsername(view)
        bindPassword(view)

        return builder.setView(view)
    }

    private fun bindUsername(view: View) {
        val usernameEditText = view.findViewById<EditText>(R.id.username)

        if (onlyShowPassword) {
            usernameEditText.visibility = GONE
        } else {
            usernameEditText.setText(username)
            usernameEditText.addTextChangedListener(object : TextWatcher {
                override fun afterTextChanged(editable: Editable) {
                    username = editable.toString()
                }

                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) = Unit

                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) = Unit
            })
        }
    }

    private fun bindPassword(view: View) {
        val passwordEditText = view.findViewById<EditText>(R.id.password)

        passwordEditText.setText(password)
        passwordEditText.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(editable: Editable) {
                password = editable.toString()
            }

            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) = Unit

            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) = Unit
        })
    }

    companion object {
        /**
         * A builder method for creating a [AuthenticationDialogFragment]
         * @param sessionId the id of the session for which this dialog will be created.
         * @param title the title of the dialog.
         * @param message the text that will go below title.
         * @param username the default value of the username text field.
         * @param password the default value of the password text field.
         * @param onlyShowPassword indicates if the dialog should include an username text field.
         */
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            title: String,
            message: String,
            username: String,
            password: String,
            onlyShowPassword: Boolean
        ): AuthenticationDialogFragment {

            val fragment = AuthenticationDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_TITLE, title)
                putString(KEY_MESSAGE, message)
                putBoolean(KEY_ONLY_SHOW_PASSWORD, onlyShowPassword)
                putString(KEY_USERNAME_EDIT_TEXT, username)
                putString(KEY_PASSWORD_EDIT_TEXT, password)
            }

            fragment.arguments = arguments
            return fragment
        }

        @StringRes
        internal val DEFAULT_TITLE = R.string.mozac_feature_prompt_sign_in
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun AlertDialog.Builder.setupTitle(): AlertDialog.Builder {
        return if (title.isEmpty()) {
            setTitle(DEFAULT_TITLE)
        } else {
            setTitle(title)
        }
    }
}
