/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginValidationDelegate
import mozilla.components.concept.storage.LoginValidationDelegate.Result
import mozilla.components.feature.prompts.R
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.ktx.android.view.toScope
import kotlin.reflect.KProperty
import com.google.android.material.R as MaterialR

private const val KEY_LOGIN_HINT = "KEY_LOGIN_HINT"
private const val KEY_LOGIN_USERNAME = "KEY_LOGIN_USERNAME"
private const val KEY_LOGIN_PASSWORD = "KEY_LOGIN_PASSWORD"
private const val KEY_LOGIN_GUID = "KEY_LOGIN_GUID"
private const val KEY_LOGIN_ORIGIN = "KEY_LOGIN_ORIGIN"
private const val KEY_LOGIN_FORM_ACTION_ORIGIN = "KEY_LOGIN_FORM_ACTION_ORIGIN"
private const val KEY_LOGIN_HTTP_REALM = "KEY_LOGIN_HTTP_REALM"

/**
 * [android.support.v4.app.DialogFragment] implementation to display a
 * dialog that allows users to save/update usernames and passwords for a given domain.
 */
@Suppress("TooManyFunctions")
internal class LoginDialogFragment : PromptDialogFragment() {

    private inner class SafeArgString(private val key: String) {
        operator fun getValue(frag: LoginDialogFragment, prop: KProperty<*>): String =
            safeArguments.getString(key)!!

        operator fun setValue(frag: LoginDialogFragment, prop: KProperty<*>, value: String) {
            safeArguments.putString(key, value)
        }
    }

    private val guid by lazy { safeArguments.getString(KEY_LOGIN_GUID) }
    private val origin by lazy { safeArguments.getString(KEY_LOGIN_ORIGIN)!! }
    private val formActionOrigin by lazy { safeArguments.getString(KEY_LOGIN_FORM_ACTION_ORIGIN) }
    private val httpRealm by lazy { safeArguments.getString(KEY_LOGIN_HTTP_REALM) }

    private var username by SafeArgString(KEY_LOGIN_USERNAME)
    private var password by SafeArgString(KEY_LOGIN_PASSWORD)

    private var stateUpdate: Job? = null

    override fun shouldDismissOnLoad(): Boolean = false

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return BottomSheetDialog(requireContext(), this.theme).apply {
            setCancelable(true)
            setOnShowListener {
                val bottomSheet =
                    findViewById<View>(MaterialR.id.design_bottom_sheet) as FrameLayout
                val behavior = BottomSheetBehavior.from(bottomSheet)
                behavior.state = BottomSheetBehavior.STATE_EXPANDED
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflateRootView(container)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.findViewById<TextView>(R.id.host_name).text = origin

        view.findViewById<TextView>(R.id.save_message).text =
            getString(R.string.mozac_feature_prompt_logins_save_message, activity?.appName)

        view.findViewById<Button>(R.id.save_confirm).setOnClickListener {
            onPositiveClickAction()
        }

        view.findViewById<Button>(R.id.save_cancel).setOnClickListener {
            dialog?.dismiss()
        }
        update()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId)
    }

    private fun onPositiveClickAction() {
        feature?.onConfirm(
            sessionId, Login(
                guid = guid,
                origin = origin,
                formActionOrigin = formActionOrigin,
                httpRealm = httpRealm,
                username = username,
                password = password
            )
        )
        dismiss()
    }

    private fun inflateRootView(container: ViewGroup? = null): View {
        val rootView = LayoutInflater.from(requireContext()).inflate(
            R.layout.mozac_feature_prompt_login_prompt,
            container,
            false
        )
        bindUsername(rootView)
        bindPassword(rootView)
        return rootView
    }

    private fun bindUsername(view: View) {
        val usernameEditText = view.findViewById<TextInputEditText>(R.id.username_field)

        usernameEditText.setText(username)
        usernameEditText.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(editable: Editable) {
                username = editable.toString()
                // Update accesses member state, so it must be called after username is set
                update()
            }

            override fun beforeTextChanged(
                s: CharSequence?,
                start: Int,
                count: Int,
                after: Int
            ) = Unit

            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) =
                Unit
        })
    }

    private fun bindPassword(view: View) {
        val passwordEditText = view.findViewById<TextInputEditText>(R.id.password_field)

        passwordEditText.setText(password)
        passwordEditText.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(editable: Editable) {
                // Note that password is accessed by `fun update`
                password = editable.toString()

                if (password.isEmpty()) {
                    setViewState(
                        confirmButtonEnabled = false,
                        passwordErrorText =
                        context?.getString(R.string.mozac_feature_prompt_error_empty_password)
                    )
                } else {
                    setViewState(
                        confirmButtonEnabled = true,
                        passwordErrorText = ""
                    )
                }
            }

            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) =
                Unit

            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) = Unit
        })
    }

    /**
     * Check current state then update view state to match.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun update() {
        val scope = view?.toScope() ?: return
        val login = Login(
            guid = guid,
            origin = origin,
            formActionOrigin = formActionOrigin,
            httpRealm = httpRealm,
            username = username,
            password = password
        )
        // The below code is not thread safe, so we ensure that we block here until we are sure that
        // the previous call has been canceled
        runBlocking { stateUpdate?.cancelAndJoin() }
        var validateDeferred: Deferred<Result>? = null
        stateUpdate = scope.launch(IO) {
            validateDeferred = feature?.loginValidationDelegate?.validateCanPersist(login)
            val result = validateDeferred?.await()

            withContext(Main) {
                when (result) {
                    is LoginValidationDelegate.Result.CanBeCreated ->
                        setViewState(
                            confirmText =
                            context?.getString(R.string.mozac_feature_prompt_save_confirmation)
                        )
                    is LoginValidationDelegate.Result.CanBeUpdated ->
                        setViewState(
                            confirmText =
                            context?.getString(R.string.mozac_feature_prompt_update_confirmation)
                        )
                    is LoginValidationDelegate.Result.Error.EmptyPassword ->
                        setViewState(
                            confirmButtonEnabled = false,
                            passwordErrorText =
                            context?.getString(R.string.mozac_feature_prompt_error_empty_password)
                        )
                    is LoginValidationDelegate.Result.Error.GeckoError -> {
                        // TODO handle these errors more robustly. See:
                        // https://github.com/mozilla-mobile/fenix/issues/7545
                        setViewState(
                            passwordErrorText =
                            context?.getString(R.string.mozac_feature_prompt_error_unknown_cause)
                        )
                    }
                }
            }
        }
        stateUpdate?.invokeOnCompletion {
            if (it is CancellationException) {
                validateDeferred?.cancel()
            }
        }
    }

    private fun setViewState(
        confirmText: String? = null,
        confirmButtonEnabled: Boolean? = null,
        passwordErrorText: String? = null
    ) {
        if (confirmText != null) {
            view?.findViewById<Button>(R.id.save_confirm)?.text = confirmText
        }

        if (confirmButtonEnabled != null) {
            view?.findViewById<Button>(R.id.save_confirm)?.isEnabled = confirmButtonEnabled
        }

        if (passwordErrorText != null) {
            view?.findViewById<TextInputLayout>(R.id.password_text_input_layout)?.error =
                passwordErrorText
        }
    }

    companion object {
        /**
         * A builder method for creating a [LoginDialogFragment]
         * @param sessionId the id of the session for which this dialog will be created.
         * @param hint a value that helps to determine the appropriate prompting behavior.
         * @param login represents login information on a given domain.
         * */
        fun newInstance(
            sessionId: String,
            hint: Int,
            login: Login
        ): LoginDialogFragment {

            val fragment = LoginDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putInt(KEY_LOGIN_HINT, hint)
                putString(KEY_LOGIN_USERNAME, login.username)
                putString(KEY_LOGIN_PASSWORD, login.password)
                putString(KEY_LOGIN_GUID, login.guid)
                putString(KEY_LOGIN_ORIGIN, login.origin)
                putString(KEY_LOGIN_FORM_ACTION_ORIGIN, login.formActionOrigin)
                putString(KEY_LOGIN_HTTP_REALM, login.httpRealm)
            }

            fragment.arguments = arguments
            return fragment
        }
    }
}
