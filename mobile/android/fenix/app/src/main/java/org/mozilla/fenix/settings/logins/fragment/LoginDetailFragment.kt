/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.fragment

import android.content.DialogInterface
import android.os.Bundle
import android.text.InputType
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import androidx.annotation.StringRes
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.databinding.FragmentLoginDetailBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.requireComponents
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
@ExperimentalCoroutinesApi
class LoginDetailFragment : Fragment(R.layout.fragment_login_detail) {

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
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_login_detail, container, false)
        _binding = FragmentLoginDetailBinding.bind(view)
        savedLoginsStore = StoreProvider.get(this) {
            LoginsFragmentStore(
                createInitialLoginsListState(requireContext().settings())
            )
        }
        loginDetailsBindingDelegate = LoginDetailsBindingDelegate(binding)

        return view
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        interactor = LoginDetailInteractor(
            SavedLoginsStorageController(
                passwordsStorage = requireContext().components.core.passwordsStorage,
                lifecycleScope = lifecycleScope,
                navController = findNavController(),
                loginsFragmentStore = savedLoginsStore
            )
        )
        interactor.onFetchLoginList(args.savedLoginId)

        consumeFrom(savedLoginsStore) {
            loginDetailsBindingDelegate.update(it)
            login = savedLoginsStore.state.currentItem
            setUpCopyButtons()
            showToolbar(
                savedLoginsStore.state.currentItem?.origin?.simplifiedUrl()
                    ?: ""
            )
            setUpPasswordReveal()
        }
        togglePasswordReveal(binding.passwordText, binding.revealPasswordButton)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        activity?.window?.setFlags(
            WindowManager.LayoutParams.FLAG_SECURE,
            WindowManager.LayoutParams.FLAG_SECURE
        )
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
            R.id.loginDetailFragment
        )
        super.onPause()
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
        binding.copyUsername.setOnClickListener(
            CopyButtonListener(login?.username, R.string.logins_username_copied)
        )

        binding.passwordText.text = login?.password
        binding.copyPassword.setOnClickListener(
            CopyButtonListener(login?.password, R.string.logins_password_copied)
        )
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.login_options_menu, menu)
        this.menu = menu
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
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

    private fun navigateToBrowser(address: String) {
        (activity as HomeActivity).openToBrowserAndLoad(
            address,
            newTab = true,
            from = BrowserDirection.FromLoginDetailFragment
        )
    }

    private fun editLogin() {
        requireComponents.analytics.metrics.track(Event.EditLogin)
        val directions =
            LoginDetailFragmentDirections.actionLoginDetailFragmentToEditLoginFragment(
                login!!
            )
        findNavController().navigate(directions)
    }

    private fun displayDeleteLoginDialog() {
        activity?.let { activity ->
            deleteDialog = AlertDialog.Builder(activity).apply {
                setMessage(R.string.login_deletion_confirmation)
                setNegativeButton(android.R.string.cancel) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
                setPositiveButton(R.string.dialog_delete_positive) { dialog: DialogInterface, _ ->
                    requireComponents.analytics.metrics.track(Event.DeleteLogin)
                    interactor.onDeleteLogin(args.savedLoginId)
                    dialog.dismiss()
                }
                create()
            }.show()
        }
    }

    /**
     * Click listener for a textview's copy button.
     * @param value Value to be copied
     * @param snackbarText Text to display in snackbar after copying.
     */
    private inner class CopyButtonListener(
        private val value: String?,
        @StringRes private val snackbarText: Int
    ) : View.OnClickListener {
        override fun onClick(view: View) {
            val clipboard = view.context.components.clipboardHandler
            clipboard.text = value
            showCopiedSnackbar(view.context.getString(snackbarText))
            view.context.components.analytics.metrics.track(Event.CopyLogin)
        }

        private fun showCopiedSnackbar(copiedItem: String) {
            view?.let {
                FenixSnackbar.make(
                    view = it,
                    duration = Snackbar.LENGTH_SHORT,
                    isDisplayedWithBrowserToolbar = false
                ).setText(copiedItem).show()
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private companion object {
        private const val BUTTON_INCREASE_DPS = 24
    }
}
