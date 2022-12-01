/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.fragment

import android.content.res.ColorStateList
import android.os.Bundle
import android.text.Editable
import android.text.InputType
import android.text.TextWatcher
import android.util.Patterns
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
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentAddLoginBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.ext.toEditable
import org.mozilla.fenix.settings.logins.LoginsFragmentStore
import org.mozilla.fenix.settings.logins.SavedLogin
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.createInitialLoginsListState
import org.mozilla.fenix.settings.logins.interactor.AddLoginInteractor

/**
 * Displays the editable new login information for a single website
 */
@Suppress("TooManyFunctions", "NestedBlockDepth", "ForbiddenComment")
class AddLoginFragment : Fragment(R.layout.fragment_add_login), MenuProvider {

    private lateinit var loginsFragmentStore: LoginsFragmentStore
    private lateinit var interactor: AddLoginInteractor

    private var duplicateLogin: SavedLogin? = null

    private var validPassword = false
    private var validUsername = true
    private var validHostname = false
    private var usernameChanged = false

    private var _binding: FragmentAddLoginBinding? = null
    private val binding get() = _binding!!

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        _binding = FragmentAddLoginBinding.bind(view)

        loginsFragmentStore = StoreProvider.get(this) {
            LoginsFragmentStore(
                createInitialLoginsListState(requireContext().settings()),
            )
        }

        interactor = AddLoginInteractor(
            SavedLoginsStorageController(
                passwordsStorage = requireContext().components.core.passwordsStorage,
                lifecycleScope = lifecycleScope,
                navController = findNavController(),
                loginsFragmentStore = loginsFragmentStore,
            ),
        )

        initEditableValues()

        setUpClickListeners()
        setUpTextListeners()
        findDuplicate()

        consumeFrom(loginsFragmentStore) {
            duplicateLogin = loginsFragmentStore.state.duplicateLogin
            updateUsernameField()
        }
    }

    private fun initEditableValues() {
        binding.hostnameText.text = "".toEditable()
        binding.usernameText.text = "".toEditable()
        binding.passwordText.text = "".toEditable()

        binding.hostnameText.inputType = InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        binding.usernameText.inputType = InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS

        // TODO: extend PasswordTransformationMethod() to change bullets to asterisks
        binding.passwordText.inputType =
            InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD

        binding.passwordText.compoundDrawablePadding =
            requireContext().resources
                .getDimensionPixelOffset(R.dimen.saved_logins_end_icon_drawable_padding)
    }

    private fun setUpClickListeners() {
        binding.hostnameText.requestFocus()
        binding.hostnameText.showKeyboard()

        binding.clearHostnameTextButton.setOnClickListener {
            binding.hostnameText.text?.clear()
            binding.hostnameText.isCursorVisible = true
            binding.hostnameText.hasFocus()
            binding.inputLayoutHostname.hasFocus()
            it.isEnabled = false
        }

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
            it.isEnabled = false
        }
    }

    private fun setUpTextListeners() {
        val frag = view?.findViewById<View>(R.id.addLoginFragment)

        frag?.onFocusChangeListener = View.OnFocusChangeListener { _, hasFocus ->
            if (hasFocus) {
                view?.hideKeyboard()
            }
        }

        binding.addLoginLayout.onFocusChangeListener = View.OnFocusChangeListener { _, hasFocus ->
            if (!hasFocus) {
                view?.hideKeyboard()
            }
        }

        binding.hostnameText.addTextChangedListener(
            object : TextWatcher {
                override fun afterTextChanged(h: Editable?) {
                    val hostnameText = h.toString()

                    when {
                        hostnameText.isEmpty() -> {
                            setHostnameError()
                            binding.clearHostnameTextButton.isEnabled = false
                        }
                        !Patterns.WEB_URL.matcher(hostnameText).matches() -> {
                            setHostnameError()
                            binding.clearHostnameTextButton.isEnabled = true
                        }
                        else -> {
                            validHostname = true

                            binding.clearHostnameTextButton.isEnabled = true
                            binding.inputLayoutHostname.error = null
                            binding.inputLayoutHostname.errorIconDrawable = null

                            findDuplicate()
                        }
                    }
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

        binding.usernameText.addTextChangedListener(
            object : TextWatcher {
                override fun afterTextChanged(editable: Editable?) {
                    // update usernameChanged to true when the text is not empty,
                    // otherwise it is not changed, as this screen starts with an empty username.
                    usernameChanged = editable.toString().isNotEmpty()
                    updateUsernameField()
                    setSaveButtonState()
                    findDuplicate()
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
                            binding.clearPasswordTextButton.isVisible = false
                            setPasswordError()
                        }
                        else -> {
                            validPassword = true
                            binding.inputLayoutPassword.error = null
                            binding.inputLayoutPassword.errorIconDrawable = null
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
            binding.hostnameText.text.toString(),
            binding.usernameText.text.toString(),
            binding.passwordText.text.toString(),
        )
    }

    private fun updateUsernameField() {
        val currentValue = binding.usernameText.text.toString()
        val layout = binding.inputLayoutUsername
        val clearButton = binding.clearUsernameTextButton
        when {
            currentValue.isEmpty() && usernameChanged -> {
                // Invalid username because it's empty (although this is not true when editing logins)
                validUsername = false
                layout.error = context?.getString(R.string.saved_login_username_required)
                layout.setErrorIconDrawable(R.drawable.mozac_ic_warning_with_bottom_padding)
                layout.setErrorIconTintList(
                    ColorStateList.valueOf(
                        ContextCompat.getColor(
                            requireContext(),
                            R.color.fx_mobile_text_color_warning,
                        ),
                    ),
                )
            }
            duplicateLogin != null -> {
                // Invalid username because it's a dupe of another login
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
            }
            else -> {
                // Valid username
                validUsername = true
                layout.error = null
                layout.errorIconDrawable = null
            }
        }
        clearButton.isVisible = validUsername
        clearButton.isEnabled = validUsername
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

    private fun setHostnameError() {
        binding.inputLayoutHostname.let { layout ->
            validHostname = false
            layout.error = context?.getString(R.string.add_login_hostname_invalid_text_2)
            layout.setErrorIconDrawable(R.drawable.mozac_ic_warning_with_bottom_padding)
            layout.setErrorIconTintList(
                ColorStateList.valueOf(
                    ContextCompat.getColor(requireContext(), R.color.fx_mobile_text_color_warning),
                ),
            )
        }
    }

    private fun setSaveButtonState() {
        activity?.invalidateMenu()
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.login_save, menu)
    }

    override fun onPrepareMenu(menu: Menu) {
        super.onPrepareMenu(menu)

        val saveButton = menu.findItem(R.id.save_login_button)
        val changesMadeWithNoErrors = validHostname && validUsername && validPassword
        saveButton.isEnabled = changesMadeWithNoErrors
    }

    override fun onPause() {
        redirectToReAuth(
            listOf(R.id.savedLoginsFragment),
            findNavController().currentDestination?.id,
            R.id.addLoginFragment,
        )
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.add_login))
    }

    override fun onMenuItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.save_login_button -> {
            view?.hideKeyboard()
            interactor.onAddLogin(
                binding.hostnameText.text.toString(),
                binding.usernameText.text.toString(),
                binding.passwordText.text.toString(),
            )
            true
        }
        else -> false
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
