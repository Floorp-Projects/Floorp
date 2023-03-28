/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.fragment

import android.content.DialogInterface
import android.os.Build
import android.os.Bundle
import android.text.InputType
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.activity.addCallback
import androidx.appcompat.app.AlertDialog
import androidx.core.os.bundleOf
import androidx.core.view.MenuProvider
import androidx.fragment.app.setFragmentResult
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.snackbar.Snackbar
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Logins
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.SecureFragment
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentLoginDetailBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.ext.simplifiedUrl
import org.mozilla.fenix.settings.logins.LoginsFragmentStore
import org.mozilla.fenix.settings.logins.SavedLogin
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.createInitialLoginsListState
import org.mozilla.fenix.settings.logins.interactor.LoginDetailInteractor
import org.mozilla.fenix.settings.logins.togglePasswordReveal
import org.mozilla.fenix.settings.logins.view.LoginDetailsBindingDelegate

/**
 * Displays saved login information for a single website.
 */
@Suppress("TooManyFunctions", "ForbiddenComment")
class LoginDetailFragment : SecureFragment(R.layout.fragment_login_detail), MenuProvider {

    private val args by navArgs<LoginDetailFragmentArgs>()
    private var login: SavedLogin? = null
    private lateinit var savedLoginsStore: LoginsFragmentStore
    private lateinit var loginDetailsBindingDelegate: LoginDetailsBindingDelegate
    private lateinit var interactor: LoginDetailInteractor
    private lateinit var menu: Menu
    private var deleteDialog: AlertDialog? = null

    private var _binding: FragmentLoginDetailBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        val view = inflater.inflate(R.layout.fragment_login_detail, container, false)
        _binding = FragmentLoginDetailBinding.bind(view)
        savedLoginsStore =
            StoreProvider.get(findNavController().getBackStackEntry(R.id.savedLogins)) {
                LoginsFragmentStore(
                    createInitialLoginsListState(requireContext().settings()),
                )
            }
        loginDetailsBindingDelegate = LoginDetailsBindingDelegate(binding)

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        interactor = LoginDetailInteractor(
            SavedLoginsStorageController(
                passwordsStorage = requireContext().components.core.passwordsStorage,
                lifecycleScope = lifecycleScope,
                navController = findNavController(),
                loginsFragmentStore = savedLoginsStore,
                clipboardHandler = requireContext().components.clipboardHandler,
            ),
        )

        interactor.onFetchLoginList(args.savedLoginId)

        consumeFrom(savedLoginsStore) {
            loginDetailsBindingDelegate.update(it)
            login = savedLoginsStore.state.currentItem
            setUpCopyButtons()
            showToolbar(
                savedLoginsStore.state.currentItem?.origin?.simplifiedUrl()
                    ?: "",
            )
            setUpPasswordReveal()
        }
        togglePasswordReveal(binding.passwordText, binding.revealPasswordButton)
    }

    /**
     * As described in #10727, the User should re-auth if the fragment is paused and the user is not
     * navigating to SavedLoginsFragment or EditLoginFragment
     *
     */
    override fun onPause() {
        deleteDialog?.isShowing.run { deleteDialog?.dismiss() }
        menu.close()
        redirectToReAuth(
            listOf(R.id.editLoginFragment, R.id.savedLoginsFragment),
            findNavController().currentDestination?.id,
            R.id.loginDetailFragment,
        )
        super.onPause()
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requireActivity().onBackPressedDispatcher.addCallback(this) {
            val directions = LoginDetailFragmentDirections.actionLoginDetailFragmentToSavedLogins()
            findNavController().navigate(directions)
        }
    }

    private fun setUpPasswordReveal() {
        binding.passwordText.inputType =
            InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
        binding.revealPasswordButton.increaseTapArea(BUTTON_INCREASE_DPS)
        binding.revealPasswordButton.setOnClickListener {
            togglePasswordReveal(binding.passwordText, binding.revealPasswordButton)
        }
        binding.passwordText.setOnClickListener {
            togglePasswordReveal(binding.passwordText, binding.revealPasswordButton)
        }
    }

    private fun setUpCopyButtons() {
        binding.webAddressText.text = login?.origin
        binding.openWebAddress.increaseTapArea(BUTTON_INCREASE_DPS)
        binding.copyUsername.increaseTapArea(BUTTON_INCREASE_DPS)
        binding.copyPassword.increaseTapArea(BUTTON_INCREASE_DPS)

        binding.openWebAddress.setOnClickListener {
            navigateToBrowser(requireNotNull(login?.origin))
        }

        binding.usernameText.text = login?.username
        binding.copyUsername.setOnClickListener {
            interactor.onCopyUsername(args.savedLoginId)
            showCopiedSnackbar(view = it, copiedItem = it.context.getString(R.string.logins_username_copied))
            Logins.copyLogin.record(NoExtras())
        }

        binding.passwordText.text = login?.password
        binding.copyPassword.setOnClickListener {
            interactor.onCopyPassword(args.savedLoginId)
            showCopiedSnackbar(view = it, copiedItem = it.context.getString(R.string.logins_password_copied))
            Logins.copyLogin.record(NoExtras())
        }
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.login_options_menu, menu)
        this.menu = menu
    }

    override fun onMenuItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.delete_login_button -> {
            displayDeleteLoginDialog()
            true
        }
        R.id.edit_login_button -> {
            editLogin()
            true
        }
        else -> false
    }

    private fun showCopiedSnackbar(view: View, copiedItem: String) {
        // Only show a toast for Android 12 and lower.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.S_V2) {
            FenixSnackbar.make(
                view,
                duration = Snackbar.LENGTH_SHORT,
                isDisplayedWithBrowserToolbar = false,
            ).setText(copiedItem).show()
        }
    }

    private fun navigateToBrowser(address: String) {
        (activity as HomeActivity).openToBrowserAndLoad(
            address,
            newTab = true,
            from = BrowserDirection.FromLoginDetailFragment,
        )
    }

    private fun editLogin() {
        Logins.openLoginEditor.record(NoExtras())
        val directions =
            LoginDetailFragmentDirections.actionLoginDetailFragmentToEditLoginFragment(
                login!!,
            )
        findNavController().navigate(directions)
    }

    private fun displayDeleteLoginDialog() {
        activity?.let { activity ->
            deleteDialog = AlertDialog.Builder(activity).apply {
                setMessage(R.string.login_deletion_confirmation)
                setNegativeButton(R.string.dialog_delete_negative) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
                setPositiveButton(R.string.dialog_delete_positive) { dialog: DialogInterface, _ ->
                    Logins.deleteSavedLogin.record(NoExtras())
                    interactor.onDeleteLogin(args.savedLoginId)
                    setFragmentResult(
                        LOGIN_REQUEST_KEY,
                        bundleOf(LOGIN_BUNDLE_ARGS to args.savedLoginId),
                    )
                    dialog.dismiss()
                }
                create().withCenterAlignedButtons()
            }.show()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        private const val BUTTON_INCREASE_DPS = 24
        const val LOGIN_REQUEST_KEY = "logins"
        const val LOGIN_BUNDLE_ARGS = "loginsBundle"
    }
}
