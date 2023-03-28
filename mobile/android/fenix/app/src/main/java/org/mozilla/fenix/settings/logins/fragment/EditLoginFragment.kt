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
import androidx.core.view.MenuProvider
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.GleanMetrics.Logins
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentEditLoginBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.redirectToReAuth
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
@Suppress("TooManyFunctions", "NestedBlockDepth", "ForbiddenComment")
class EditLoginFragment : Fragment(R.layout.fragment_edit_login), MenuProvider {

    private val args by navArgs<EditLoginFragmentArgs>()
    private lateinit var loginsFragmentStore: LoginsFragmentStore
    private lateinit var interactor: EditLoginInteractor
    private lateinit var oldLogin: SavedLogin

    private var duplicateLogin: SavedLogin? = null

    private var usernameChanged = false
    private var passwordChanged = false

    private var validPassword = true
    private var validUsername = true

    private var _binding: FragmentEditLoginBinding? = null
    private val binding get() = _binding!!

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        _binding = FragmentEditLoginBinding.bind(view)

        oldLogin = args.savedLoginItem

        loginsFragmentStore =
            StoreProvider.get(findNavController().getBackStackEntry(R.id.savedLogins)) {
                LoginsFragmentStore(
                    createInitialLoginsListState(requireContext().settings()),
                )
            }

        interactor = EditLoginInteractor(
            SavedLoginsStorageController(
                passwordsStorage = requireContext().components.core.passwordsStorage,
                lifecycleScope = lifecycleScope,
                navController = findNavController(),
                loginsFragmentStore = loginsFragmentStore,
                clipboardHandler = requireContext().components.clipboardHandler,
            ),
        )

        loginsFragmentStore.dispatch(LoginsAction.UpdateCurrentLogin(args.savedLoginItem))

        // initialize editable values
        binding.hostnameText.text = args.savedLoginItem.origin.toEditable()
        binding.usernameText.text = args.savedLoginItem.username.toEditable()
        binding.passwordText.text = args.savedLoginItem.password.toEditable()

        binding.clearUsernameTextButton.isEnabled = oldLogin.username.isNotEmpty()

        formatEditableValues()
        setUpClickListeners()
        setUpTextListeners()
        togglePasswordReveal(binding.passwordText, binding.revealPasswordButton)
        findDuplicate()

        consumeFrom(loginsFragmentStore) {
            duplicateLogin = loginsFragmentStore.state.duplicateLogin
            updateUsernameField()
        }
    }

    private fun formatEditableValues() {
        binding.hostnameText.isClickable = false
        binding.hostnameText.isFocusable = false
        binding.usernameText.inputType = InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        // TODO: extend PasswordTransformationMethod() to change bullets to asterisks
        binding.passwordText.inputType =
            InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
        binding.passwordText.compoundDrawablePadding =
            requireContext().resources
                .getDimensionPixelOffset(R.dimen.saved_logins_end_icon_drawable_padding)
    }

    private fun setUpClickListeners() {
        binding.clearUsernameTextButton.setOnClickListener {
            binding.usernameText.text?.clear()
            binding.usernameText.isCursorVisible = true
            binding.usernameText.hasFocus()
            binding.inputLayoutUsername.hasFocus()
            it.isEnabled = false
        }
        binding.clearPasswordTextButton.setOnClickListener {
            binding.passwordText.text?.clear()
            binding.passwordText.isCursorVisible = true
            binding.passwordText.hasFocus()
            binding.inputLayoutPassword.hasFocus()
        }
        binding.revealPasswordButton.setOnClickListener {
            togglePasswordReveal(binding.passwordText, binding.revealPasswordButton)
        }
    }

    private fun setUpTextListeners() {
        val frag = view?.findViewById<View>(R.id.editLoginFragment)
        frag?.onFocusChangeListener = View.OnFocusChangeListener { _, hasFocus ->
            if (hasFocus) {
                view?.hideKeyboard()
            }
        }
        binding.editLoginLayout.onFocusChangeListener = View.OnFocusChangeListener { _, hasFocus ->
            if (!hasFocus) {
                view?.hideKeyboard()
            }
        }

        binding.usernameText.addTextChangedListener(
            object : TextWatcher {
                override fun afterTextChanged(u: Editable?) {
                    updateUsernameField()
                    findDuplicate()
                    setSaveButtonState()
                }

                override fun beforeTextChanged(
                    u: CharSequence?,
                    start: Int,
                    count: Int,
                    after: Int,
                ) {
                    // NOOP
                }

                override fun onTextChanged(u: CharSequence?, start: Int, before: Int, count: Int) {
                    // NOOP
                }
            },
        )

        binding.passwordText.addTextChangedListener(
            object : TextWatcher {
                override fun afterTextChanged(p: Editable?) {
                    when {
                        p.toString().isEmpty() -> {
                            passwordChanged = true
                            binding.revealPasswordButton.isVisible = false
                            binding.clearPasswordTextButton.isVisible = false
                            setPasswordError()
                        }
                        p.toString() == oldLogin.password -> {
                            passwordChanged = false
                            validPassword = true
                            binding.inputLayoutPassword.error = null
                            binding.inputLayoutPassword.errorIconDrawable = null
                            binding.revealPasswordButton.isVisible = true
                            binding.clearPasswordTextButton.isVisible = true
                        }
                        else -> {
                            passwordChanged = true
                            validPassword = true
                            binding.inputLayoutPassword.error = null
                            binding.inputLayoutPassword.errorIconDrawable = null
                            binding.revealPasswordButton.isVisible = true
                            binding.clearPasswordTextButton.isVisible = true
                        }
                    }
                    setSaveButtonState()
                }

                override fun beforeTextChanged(
                    p: CharSequence?,
                    start: Int,
                    count: Int,
                    after: Int,
                ) {
                    // NOOP
                }

                override fun onTextChanged(p: CharSequence?, start: Int, before: Int, count: Int) {
                    // NOOP
                }
            },
        )
    }

    private fun findDuplicate() {
        interactor.findDuplicate(
            oldLogin.guid,
            binding.usernameText.text.toString(),
            binding.passwordText.text.toString(),
        )
    }

    private fun updateUsernameField() {
        val currentValue = binding.usernameText.text.toString()
        val layout = binding.inputLayoutUsername
        val clearButton = binding.clearUsernameTextButton
        when {
            (duplicateLogin == null || oldLogin.username == currentValue) -> {
                // Valid login, either because there's no dupe or because the
                // existing login was already a dupe and the username hasn't
                // changed
                usernameChanged = oldLogin.username != currentValue
                validUsername = true
                layout.error = null
                layout.errorIconDrawable = null
                clearButton.isVisible = true
            }
            else -> {
                // Invalid login because it's a dupe of another one
                usernameChanged = true
                validUsername = false
                layout.error = context?.getString(R.string.saved_login_duplicate)
                layout.setErrorIconDrawable(R.drawable.mozac_ic_warning_with_bottom_padding)
                layout.setErrorIconTintList(
                    ColorStateList.valueOf(
                        ContextCompat.getColor(
                            requireContext(),
                            R.color.fx_mobile_text_color_warning,
                        ),
                    ),
                )
                clearButton.isVisible = false
            }
        }
        clearButton.isEnabled = currentValue.isNotEmpty()
        setSaveButtonState()
    }

    private fun setPasswordError() {
        binding.inputLayoutPassword.let { layout ->
            validPassword = false
            layout.error = context?.getString(R.string.saved_login_password_required)
            layout.setErrorIconDrawable(R.drawable.mozac_ic_warning_with_bottom_padding)
            layout.setErrorIconTintList(
                ColorStateList.valueOf(
                    ContextCompat.getColor(requireContext(), R.color.fx_mobile_text_color_warning),
                ),
            )
        }
    }

    private fun setSaveButtonState() {
        activity?.invalidateOptionsMenu()
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.login_save, menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu) {
        val saveButton = menu.findItem(R.id.save_login_button)
        val changesMadeWithNoErrors =
            validUsername && validPassword && (usernameChanged || passwordChanged)
        saveButton.isEnabled =
            changesMadeWithNoErrors // don't enable saving until something has been changed
    }

    override fun onPause() {
        redirectToReAuth(
            listOf(R.id.loginDetailFragment, R.id.savedLoginsFragment),
            findNavController().currentDestination?.id,
            R.id.editLoginFragment,
        )
        super.onPause()
    }

    override fun onMenuItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.save_login_button -> {
            view?.hideKeyboard()
            interactor.onSaveLogin(
                args.savedLoginItem.guid,
                binding.usernameText.text.toString(),
                binding.passwordText.text.toString(),
            )
            Logins.saveEditedLogin.record(NoExtras())
            true
        }
        else -> false
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
