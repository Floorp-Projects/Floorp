/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.fragment

import android.content.res.ColorStateList
import android.os.Bundle
import android.text.Editable
import android.text.InputType
import android.text.TextWatcher
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_edit_login.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.toEditable
import org.mozilla.fenix.settings.logins.LoginsAction
import org.mozilla.fenix.settings.logins.LoginsFragmentStore
import org.mozilla.fenix.settings.logins.SavedLogin
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.createInitialLoginsListState
import org.mozilla.fenix.settings.logins.interactor.EditLoginInteractor
import org.mozilla.fenix.settings.logins.togglePasswordReveal

/**
 * Displays the editable saved login information for a single website
 */
@ExperimentalCoroutinesApi
@Suppress("TooManyFunctions", "NestedBlockDepth", "ForbiddenComment")
class EditLoginFragment : Fragment(R.layout.fragment_edit_login) {

    private val args by navArgs<EditLoginFragmentArgs>()
    private lateinit var loginsFragmentStore: LoginsFragmentStore
    private lateinit var interactor: EditLoginInteractor
    private lateinit var oldLogin: SavedLogin

    private var listOfPossibleDupes: List<SavedLogin>? = null

    private var usernameChanged = false
    private var passwordChanged = false

    private var validPassword = true
    private var validUsername = true

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setHasOptionsMenu(true)
        oldLogin = args.savedLoginItem

        loginsFragmentStore = StoreProvider.get(this) {
            LoginsFragmentStore(
                createInitialLoginsListState(requireContext().settings())
            )
        }

        interactor = EditLoginInteractor(
            SavedLoginsStorageController(
                passwordsStorage = requireContext().components.core.passwordsStorage,
                lifecycleScope = lifecycleScope,
                navController = findNavController(),
                loginsFragmentStore = loginsFragmentStore
            )
        )

        loginsFragmentStore.dispatch(LoginsAction.UpdateCurrentLogin(args.savedLoginItem))
        interactor.findPotentialDuplicates(args.savedLoginItem.guid)

        // initialize editable values
        hostnameText.text = args.savedLoginItem.origin.toEditable()
        usernameText.text = args.savedLoginItem.username.toEditable()
        passwordText.text = args.savedLoginItem.password.toEditable()

        clearUsernameTextButton.isEnabled = oldLogin.username.isNotEmpty()

        formatEditableValues()
        setUpClickListeners()
        setUpTextListeners()
        togglePasswordReveal(passwordText, revealPasswordButton)

        consumeFrom(loginsFragmentStore) {
            listOfPossibleDupes = loginsFragmentStore.state.duplicateLogins
        }
    }

    private fun formatEditableValues() {
        hostnameText.isClickable = false
        hostnameText.isFocusable = false
        usernameText.inputType = InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        // TODO: extend PasswordTransformationMethod() to change bullets to asterisks
        passwordText.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
        passwordText.compoundDrawablePadding =
            requireContext().resources
                .getDimensionPixelOffset(R.dimen.saved_logins_end_icon_drawable_padding)
    }

    private fun setUpClickListeners() {
        clearUsernameTextButton.setOnClickListener {
            usernameText.text?.clear()
            usernameText.isCursorVisible = true
            usernameText.hasFocus()
            inputLayoutUsername.hasFocus()
            it.isEnabled = false
        }
        clearPasswordTextButton.setOnClickListener {
            passwordText.text?.clear()
            passwordText.isCursorVisible = true
            passwordText.hasFocus()
            inputLayoutPassword.hasFocus()
        }
        revealPasswordButton.setOnClickListener {
            togglePasswordReveal(passwordText, revealPasswordButton)
        }
    }

    private fun setUpTextListeners() {
        val frag = view?.findViewById<View>(R.id.editLoginFragment)
        frag?.onFocusChangeListener = View.OnFocusChangeListener { _, hasFocus ->
            if (hasFocus) {
                view?.hideKeyboard()
            }
        }
        editLoginLayout.onFocusChangeListener = View.OnFocusChangeListener { _, hasFocus ->
            if (!hasFocus) {
                view?.hideKeyboard()
            }
        }

        usernameText.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(u: Editable?) {
                when (oldLogin.username) {
                    u.toString() -> {
                        usernameChanged = false
                        validUsername = true
                        inputLayoutUsername.error = null
                        inputLayoutUsername.errorIconDrawable = null
                        clearUsernameTextButton.isVisible = true
                    }
                    else -> {
                        usernameChanged = true
                        setDupeError()
                    }
                }
                clearUsernameTextButton.isEnabled = u.toString().isNotEmpty()
                setSaveButtonState()
            }

            override fun beforeTextChanged(u: CharSequence?, start: Int, count: Int, after: Int) {
                // NOOP
            }

            override fun onTextChanged(u: CharSequence?, start: Int, before: Int, count: Int) {
                // NOOP
            }
        })

        passwordText.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(p: Editable?) {
                when {
                    p.toString().isEmpty() -> {
                        passwordChanged = true
                        revealPasswordButton.isVisible = false
                        clearPasswordTextButton.isVisible = false
                        setPasswordError()
                    }
                    p.toString() == oldLogin.password -> {
                        passwordChanged = false
                        validPassword = true
                        inputLayoutPassword.error = null
                        inputLayoutPassword.errorIconDrawable = null
                        revealPasswordButton.isVisible = true
                        clearPasswordTextButton.isVisible = true
                    }
                    else -> {
                        passwordChanged = true
                        validPassword = true
                        inputLayoutPassword.error = null
                        inputLayoutPassword.errorIconDrawable = null
                        revealPasswordButton.isVisible = true
                        clearPasswordTextButton.isVisible = true
                    }
                }
                setSaveButtonState()
            }

            override fun beforeTextChanged(p: CharSequence?, start: Int, count: Int, after: Int) {
                // NOOP
            }

            override fun onTextChanged(p: CharSequence?, start: Int, before: Int, count: Int) {
                // NOOP
            }
        })
    }

    private fun isDupe(username: String): Boolean =
        loginsFragmentStore.state.duplicateLogins.filter { it.username == username }.any()

    private fun setDupeError() {
        if (isDupe(usernameText.text.toString())) {
            inputLayoutUsername?.let {
                usernameChanged = true
                validUsername = false
                it.error = context?.getString(R.string.saved_login_duplicate)
                it.setErrorIconDrawable(R.drawable.mozac_ic_warning_with_bottom_padding)
                it.setErrorIconTintList(
                    ColorStateList.valueOf(
                        ContextCompat.getColor(requireContext(), R.color.design_error)
                    )
                )
                clearUsernameTextButton.isVisible = false
            }
        } else {
            usernameChanged = true
            validUsername = true
            inputLayoutUsername.error = null
            inputLayoutUsername.errorIconDrawable = null
            clearUsernameTextButton.isVisible = true
        }
    }

    private fun setPasswordError() {
        inputLayoutPassword?.let { layout ->
            validPassword = false
            layout.error = context?.getString(R.string.saved_login_password_required)
            layout.setErrorIconDrawable(R.drawable.mozac_ic_warning_with_bottom_padding)
            layout.setErrorIconTintList(
                ColorStateList.valueOf(
                    ContextCompat.getColor(requireContext(), R.color.design_error)
                )
            )
        }
    }

    private fun setSaveButtonState() {
        activity?.invalidateOptionsMenu()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.login_save, menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu) {
        val saveButton = menu.findItem(R.id.save_login_button)
        val changesMadeWithNoErrors =
            validUsername && validPassword && (usernameChanged || passwordChanged)
        saveButton.isEnabled = changesMadeWithNoErrors // don't enable saving until something has been changed
    }

    override fun onPause() {
        redirectToReAuth(
            listOf(R.id.loginDetailFragment),
            findNavController().currentDestination?.id,
            R.id.editLoginFragment
        )
        super.onPause()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.save_login_button -> {
            view?.hideKeyboard()
            interactor.onSaveLogin(
                args.savedLoginItem.guid,
                usernameText.text.toString(),
                passwordText.text.toString()
            )
            requireComponents.analytics.metrics.track(Event.EditLoginSave)
            true
        }
        else -> false
    }
}
