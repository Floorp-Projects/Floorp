/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.app.Dialog
import android.content.DialogInterface
import android.content.res.ColorStateList
import android.graphics.Bitmap
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ImageView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.content.ContextCompat
import androidx.core.widget.ImageViewCompat
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.concept.storage.LoginValidationDelegate.Result
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.ext.onDone
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.toScope
import kotlin.reflect.KProperty
import com.google.android.material.R as MaterialR

private const val KEY_LOGIN_HINT = "KEY_LOGIN_HINT"
private const val KEY_LOGIN_USERNAME = "KEY_LOGIN_USERNAME"
private const val KEY_LOGIN_PASSWORD = "KEY_LOGIN_PASSWORD"
private const val KEY_LOGIN_ORIGIN = "KEY_LOGIN_ORIGIN"
private const val KEY_LOGIN_FORM_ACTION_ORIGIN = "KEY_LOGIN_FORM_ACTION_ORIGIN"
private const val KEY_LOGIN_HTTP_REALM = "KEY_LOGIN_HTTP_REALM"

@VisibleForTesting internal const val KEY_LOGIN_ICON = "KEY_LOGIN_ICON"

/**
 * [android.support.v4.app.DialogFragment] implementation to display a
 * dialog that allows users to save/update usernames and passwords for a given domain.
 */
@Suppress("LargeClass")
internal class SaveLoginDialogFragment : PromptDialogFragment() {

    private inner class SafeArgString(private val key: String) {
        operator fun getValue(frag: SaveLoginDialogFragment, prop: KProperty<*>): String =
            safeArguments.getString(key)!!

        operator fun setValue(frag: SaveLoginDialogFragment, prop: KProperty<*>, value: String) {
            safeArguments.putString(key, value)
        }
    }

    private val origin by lazy { safeArguments.getString(KEY_LOGIN_ORIGIN)!! }
    private val formActionOrigin by lazy { safeArguments.getString(KEY_LOGIN_FORM_ACTION_ORIGIN) }
    private val httpRealm by lazy { safeArguments.getString(KEY_LOGIN_HTTP_REALM) }

    @VisibleForTesting
    internal val icon by lazy { safeArguments.getParcelable<Bitmap>(KEY_LOGIN_ICON) }

    @VisibleForTesting
    internal var username by SafeArgString(KEY_LOGIN_USERNAME)

    @VisibleForTesting
    internal var password by SafeArgString(KEY_LOGIN_PASSWORD)

    @Volatile
    private var loginValid = false
    private var validateStateUpdate: Job? = null

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return BottomSheetDialog(requireContext(), R.style.MozDialogStyle).apply {
            setCancelable(true)
            setOnShowListener {
                /*
                 Note: we must include a short delay before expanding the bottom sheet.
                 This is because the keyboard is still in the process of hiding when `onShowListener` is triggered.
                 Because of this, we'll only be given a small portion of the screen to draw on which will set the bottom
                 anchor of this view incorrectly to somewhere in the center of the view. If we delay a small amount we
                 are given the correct amount of space and are properly anchored.
                 */
                CoroutineScope(IO).launch {
                    delay(KEYBOARD_HIDING_DELAY)
                    launch(Main) {
                        val bottomSheet =
                            findViewById<View>(MaterialR.id.design_bottom_sheet) as FrameLayout
                        val behavior = BottomSheetBehavior.from(bottomSheet)
                        behavior.state = BottomSheetBehavior.STATE_EXPANDED
                    }
                }
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        /*
         * If an implementation of [LoginExceptions] is hooked up to [PromptFeature], we will not
         * show this save login dialog for any origin saved as an exception.
         */
        CoroutineScope(IO).launch {
            if (feature?.loginExceptionStorage?.isLoginExceptionByOrigin(origin) == true) {
                feature?.onCancel(sessionId, promptRequestUID)
                dismiss()
            }
        }

        return setupRootView(container)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.findViewById<AppCompatTextView>(R.id.host_name).text = origin

        view.findViewById<AppCompatTextView>(R.id.save_message).text =
            getString(R.string.mozac_feature_prompt_login_save_headline)

        view.findViewById<Button>(R.id.save_confirm).setOnClickListener {
            onPositiveClickAction()
        }

        view.findViewById<Button>(R.id.save_cancel).apply {
            setOnClickListener {
                if (this.text == context?.getString(R.string.mozac_feature_prompt_never_save)) {
                    emitNeverSaveFact()
                    CoroutineScope(IO).launch {
                        feature?.loginExceptionStorage?.addLoginException(origin)
                    }
                }
                feature?.onCancel(sessionId, promptRequestUID)
                dismiss()
            }
        }

        emitDisplayFact()
        update()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId, promptRequestUID)
        emitCancelFact()
    }

    private fun onPositiveClickAction() {
        feature?.onConfirm(
            sessionId,
            promptRequestUID,
            LoginEntry(
                origin = origin,
                formActionOrigin = formActionOrigin,
                httpRealm = httpRealm,
                username = username,
                password = password,
            ),
        )
        emitSaveFact()
        dismiss()
    }

    @VisibleForTesting
    internal fun setupRootView(container: ViewGroup? = null): View {
        val rootView = inflateRootView(container)
        bindUsername(rootView)
        bindPassword(rootView)
        bindIcon(rootView)
        return rootView
    }

    @VisibleForTesting
    internal fun inflateRootView(container: ViewGroup? = null): View {
        return LayoutInflater.from(requireContext()).inflate(
            R.layout.mozac_feature_prompt_save_login_prompt,
            container,
            false,
        )
    }

    private fun bindUsername(view: View) {
        val usernameEditText = view.findViewById<TextInputEditText>(R.id.username_field)

        usernameEditText.setText(username)
        usernameEditText.addTextChangedListener(
            object : TextWatcher {
                override fun afterTextChanged(editable: Editable) {
                    username = editable.toString()
                    // Update accesses member state, so it must be called after username is set
                    update()
                }

                override fun beforeTextChanged(
                    s: CharSequence?,
                    start: Int,
                    count: Int,
                    after: Int,
                ) = Unit

                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) =
                    Unit
            },
        )

        with(usernameEditText) {
            onDone(false) {
                hideKeyboard()
                clearFocus()
            }
        }
    }

    private fun bindPassword(view: View) {
        val passwordEditText = view.findViewById<TextInputEditText>(R.id.password_field)

        passwordEditText.addTextChangedListener(
            object : TextWatcher {
                override fun afterTextChanged(editable: Editable) {
                    // Note that password is accessed by `fun update`
                    password = editable.toString()
                    if (password.isEmpty()) {
                        setViewState(
                            loginValid = false,
                            passwordErrorText =
                            context?.getString(R.string.mozac_feature_prompt_error_empty_password),
                        )
                    } else {
                        setViewState(
                            loginValid = true,
                            passwordErrorText = "",
                        )
                    }
                }

                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) =
                    Unit

                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) = Unit
            },
        )
        passwordEditText.setText(password)

        with(passwordEditText) {
            onDone(false) {
                hideKeyboard()
                clearFocus()
            }
        }
    }

    private fun bindIcon(view: View) {
        val iconView = view.findViewById<ImageView>(R.id.host_icon)
        if (icon != null) {
            iconView.setImageBitmap(icon)
        } else {
            setImageViewTint(iconView)
        }
    }

    @VisibleForTesting
    internal fun setImageViewTint(imageView: ImageView) {
        val tintColor = ContextCompat.getColor(
            requireContext(),
            requireContext().theme.resolveAttribute(android.R.attr.textColorPrimary),
        )
        ImageViewCompat.setImageTintList(imageView, ColorStateList.valueOf(tintColor))
    }

    /**
     * Check current state then update view state to match.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun update() = view?.toScope()?.launch(IO) {
        val entry = LoginEntry(
            origin = origin,
            formActionOrigin = formActionOrigin,
            httpRealm = httpRealm,
            username = username,
            password = password,
        )

        try {
            validateStateUpdate?.cancelAndJoin()
        } catch (cancellationException: CancellationException) {
            Logger.error("Failed to cancel job", cancellationException)
        }

        var validateDeferred: Deferred<Result>?
        validateStateUpdate = launch validate@{
            if (!loginValid) {
                // Don't run the validation logic if we know the login is invalid
                return@validate
            }
            val validationDelegate =
                feature?.loginValidationDelegate ?: return@validate
            validateDeferred = validationDelegate.shouldUpdateOrCreateAsync(entry)
            val result = validateDeferred?.await()
            withContext(Main) {
                when (result) {
                    Result.CanBeCreated -> {
                        setViewState(
                            headline = context?.getString(R.string.mozac_feature_prompt_login_save_headline),
                            negativeText = context?.getString(R.string.mozac_feature_prompt_never_save),
                            confirmText = context?.getString(R.string.mozac_feature_prompt_save_confirmation),
                        )
                    }
                    is Result.CanBeUpdated -> {
                        setViewState(
                            headline = if (result.foundLogin.username.isEmpty()) {
                                context?.getString(
                                    R.string.mozac_feature_prompt_login_add_username_headline,
                                )
                            } else {
                                context?.getString(R.string.mozac_feature_prompt_login_update_headline)
                            },
                            negativeText = context?.getString(R.string.mozac_feature_prompt_dont_update),
                            confirmText =
                            context?.getString(R.string.mozac_feature_prompt_update_confirmation),
                        )
                    }
                    else -> {
                        // no-op
                    }
                }
            }
            validateStateUpdate?.invokeOnCompletion {
                if (it is CancellationException) {
                    validateDeferred?.cancel()
                }
            }
        }
    }

    private fun setViewState(
        headline: String? = null,
        negativeText: String? = null,
        confirmText: String? = null,
        loginValid: Boolean? = null,
        passwordErrorText: String? = null,
    ) {
        if (headline != null) {
            view?.findViewById<AppCompatTextView>(R.id.save_message)?.text = headline
        }

        if (negativeText != null) {
            view?.findViewById<MaterialButton>(R.id.save_cancel)?.text = negativeText
        }

        val confirmButton = view?.findViewById<Button>(R.id.save_confirm)
        if (confirmText != null) {
            confirmButton?.text = confirmText
        }

        if (loginValid != null) {
            this.loginValid = loginValid
            confirmButton?.isEnabled = loginValid
        }

        if (passwordErrorText != null) {
            view?.findViewById<TextInputLayout>(R.id.password_text_input_layout)?.error =
                passwordErrorText
        }
    }

    companion object {
        private const val KEYBOARD_HIDING_DELAY = 100L

        /**
         * A builder method for creating a [SaveLoginDialogFragment]
         * @param sessionId the id of the session for which this dialog will be created.
         * @param promptRequestUID identifier of the [PromptRequest] for which this dialog is shown.
         * @param shouldDismissOnLoad whether or not the dialog should automatically be dismissed
         * when a new page is loaded.
         * @param hint a value that helps to determine the appropriate prompting behavior.
         * @param login represents login information on a given domain.
         * */
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            shouldDismissOnLoad: Boolean,
            hint: Int,
            entry: LoginEntry,
            icon: Bitmap? = null,
        ): SaveLoginDialogFragment {
            val fragment = SaveLoginDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putInt(KEY_LOGIN_HINT, hint)
                putString(KEY_LOGIN_USERNAME, entry.username)
                putString(KEY_LOGIN_PASSWORD, entry.password)
                putString(KEY_LOGIN_ORIGIN, entry.origin)
                putString(KEY_LOGIN_FORM_ACTION_ORIGIN, entry.formActionOrigin)
                putString(KEY_LOGIN_HTTP_REALM, entry.httpRealm)
                putParcelable(KEY_LOGIN_ICON, icon)
            }

            fragment.arguments = arguments
            return fragment
        }
    }
}
