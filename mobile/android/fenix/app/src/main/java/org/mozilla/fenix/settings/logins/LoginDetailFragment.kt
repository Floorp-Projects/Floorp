/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.content.DialogInterface
import android.os.Bundle
import android.text.InputType
import android.view.MenuItem
import android.view.MenuInflater
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import android.view.LayoutInflater
import androidx.annotation.StringRes
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_login_detail.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch
import kotlinx.coroutines.async
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.Login
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.ext.urlToTrimmedHost

/**
 * Displays saved login information for a single website.
 */
@Suppress("TooManyFunctions", "ForbiddenComment")
@ExperimentalCoroutinesApi
class LoginDetailFragment : Fragment(R.layout.fragment_login_detail) {

    private val args by navArgs<LoginDetailFragmentArgs>()
    private var login: SavedLogin? = null
    private lateinit var savedLoginsStore: LoginsFragmentStore
    private lateinit var loginDetailView: LoginDetailView
    private lateinit var menu: Menu
    private var deleteDialog: AlertDialog? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_login_detail, container, false)
        savedLoginsStore = StoreProvider.get(this) {
            LoginsFragmentStore(
                LoginsListState(
                    isLoading = true,
                    loginList = listOf(),
                    filteredItems = listOf(),
                    searchedForText = null,
                    sortingStrategy = requireContext().settings().savedLoginsSortingStrategy,
                    highlightedItem = requireContext().settings().savedLoginsMenuHighlightedItem
                )
            )
        }
        loginDetailView = LoginDetailView(view?.findViewById(R.id.loginDetailLayout))
        fetchLoginDetails()

        return view
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        consumeFrom(savedLoginsStore) {
            loginDetailView.update(it)
            login = savedLoginsStore.state.currentItem
            setUpCopyButtons()
            showToolbar(
                savedLoginsStore.state.currentItem?.origin?.urlToTrimmedHost(requireContext())
                    ?: ""
            )
            setUpPasswordReveal()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
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
            findNavController().currentDestination?.id
        )
        super.onPause()
    }

    private fun setUpPasswordReveal() {
        passwordText.inputType =
            InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
        revealPasswordButton.setOnClickListener {
            togglePasswordReveal()
        }
    }

    private fun setUpCopyButtons() {
        webAddressText.text = login?.origin
        copyWebAddress.setOnClickListener(
            CopyButtonListener(login?.origin, R.string.logins_site_copied)
        )

        usernameText.text = login?.username
        copyUsername.setOnClickListener(
            CopyButtonListener(login?.username, R.string.logins_username_copied)
        )

        passwordText.text = login?.password
        copyPassword.setOnClickListener(
            CopyButtonListener(login?.password, R.string.logins_password_copied)
        )
    }

    // TODO: Move interactions with the component's password storage into a separate datastore
    private fun fetchLoginDetails() {
        var deferredLogin: Deferred<List<Login>>? = null
        val fetchLoginJob = viewLifecycleOwner.lifecycleScope.launch(IO) {
            deferredLogin = async {
                requireContext().components.core.passwordsStorage.list()
            }
            val fetchedLoginList = deferredLogin?.await()

            fetchedLoginList?.let {
                withContext(Main) {
                    val login = fetchedLoginList.filter {
                        it.guid == args.savedLoginId
                    }.first()
                    savedLoginsStore.dispatch(
                        LoginsAction.UpdateCurrentLogin(login.mapToSavedLogin())
                    )
                }
            }
        }
        fetchLoginJob.invokeOnCompletion {
            if (it is CancellationException) {
                deferredLogin?.cancel()
            }
        }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        if (FeatureFlags.loginsEdit) {
            inflater.inflate(R.menu.login_options_menu, menu)
        } else {
            inflater.inflate(R.menu.login_delete, menu)
        }
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

    private fun editLogin() {
        val directions =
            LoginDetailFragmentDirections
                .actionLoginDetailFragmentToEditLoginFragment(login!!)
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
                    deleteLogin()
                    dialog.dismiss()
                }
                create()
            }.show()
        }
    }

    // TODO: Move interactions with the component's password storage into a separate datastore
    // This includes Delete, Update/Edit, Create
    private fun deleteLogin() {
        var deleteLoginJob: Deferred<Boolean>? = null
        val deleteJob = viewLifecycleOwner.lifecycleScope.launch(IO) {
            deleteLoginJob = async {
                requireContext().components.core.passwordsStorage.delete(args.savedLoginId)
            }
            deleteLoginJob?.await()
            withContext(Main) {
                findNavController().popBackStack(R.id.savedLoginsFragment, false)
            }
        }
        deleteJob.invokeOnCompletion {
            if (it is CancellationException) {
                deleteLoginJob?.cancel()
            }
        }
    }

    // TODO: create helper class for toggling passwords. Used in login info and edit fragments.
    private fun togglePasswordReveal() {
        if (passwordText.inputType == InputType.TYPE_TEXT_VARIATION_PASSWORD or InputType.TYPE_CLASS_TEXT) {
            context?.components?.analytics?.metrics?.track(Event.ViewLoginPassword)
            passwordText.inputType = InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            revealPasswordButton.setImageDrawable(
                resources.getDrawable(R.drawable.mozac_ic_password_hide, null)
            )
            revealPasswordButton.contentDescription =
                resources.getString(R.string.saved_login_hide_password)
        } else {
            passwordText.inputType =
                InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
            revealPasswordButton.setImageDrawable(
                resources.getDrawable(R.drawable.mozac_ic_password_reveal, null)
            )
            revealPasswordButton.contentDescription =
                context?.getString(R.string.saved_login_reveal_password)
        }
        // For the new type to take effect you need to reset the text
        passwordText.text = login?.password
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
}
