/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.controller

import android.util.Log
import androidx.navigation.NavController
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.EncryptedLogin
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.service.sync.logins.InvalidRecordException
import mozilla.components.service.sync.logins.LoginsApiException
import mozilla.components.service.sync.logins.NoSuchRecordException
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.logins.LoginsAction
import org.mozilla.fenix.settings.logins.LoginsFragmentStore
import org.mozilla.fenix.settings.logins.fragment.AddLoginFragmentDirections
import org.mozilla.fenix.settings.logins.fragment.EditLoginFragmentDirections
import org.mozilla.fenix.settings.logins.mapToSavedLogin
import org.mozilla.fenix.utils.ClipboardHandler

/**
 * Controller for all saved logins interactions with the password storage component
 */
@Suppress("TooManyFunctions", "LargeClass")
open class SavedLoginsStorageController(
    private val passwordsStorage: SyncableLoginsStorage,
    private val lifecycleScope: CoroutineScope,
    private val navController: NavController,
    private val loginsFragmentStore: LoginsFragmentStore,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
    private val clipboardHandler: ClipboardHandler,
) {

    fun delete(loginId: String) {
        var deleteLoginJob: Deferred<Boolean>? = null
        val deleteJob = lifecycleScope.launch(ioDispatcher) {
            deleteLoginJob = async {
                passwordsStorage.delete(loginId)
            }
            deleteLoginJob?.await()
            deleteLoginFromState(loginId)
            withContext(Dispatchers.Main) {
                navController.popBackStack(R.id.savedLoginsFragment, false)
            }
        }
        deleteJob.invokeOnCompletion {
            if (it is CancellationException) {
                deleteLoginJob?.cancel()
            }
        }
    }

    // Create a [LoginEntry] for the add login dialog
    private fun loginEntryForAdd(originText: String, usernameText: String, passwordText: String) = LoginEntry(
        origin = originText,
        username = usernameText,
        password = passwordText,
        // Implicitly fill in httpRealm with the origin
        httpRealm = originText,
    )

    fun add(originText: String, usernameText: String, passwordText: String) {
        var saveLoginJob: Deferred<Unit>? = null
        var id: String? = null
        lifecycleScope.launch(ioDispatcher) {
            saveLoginJob = async {
                id = add(loginEntryForAdd(originText, usernameText, passwordText))
            }
            saveLoginJob?.await()
            withContext(Dispatchers.Main) {
                if (id.isNullOrEmpty()) {
                    navController.popBackStack(R.id.savedLoginsFragment, false)
                } else {
                    val directions =
                        AddLoginFragmentDirections.actionAddLoginFragmentToLoginDetailFragment(id.toString())
                    navController.navigate(directions)
                }
            }
        }
        saveLoginJob?.invokeOnCompletion {
            if (it is CancellationException) {
                saveLoginJob?.cancel()
            }
        }
    }

    private suspend fun add(loginEntryToSave: LoginEntry): String? {
        var encryptedLogin: EncryptedLogin? = null
        try {
            encryptedLogin = passwordsStorage.add(loginEntryToSave)
            addLoginToState(passwordsStorage.decryptLogin(encryptedLogin))
        } catch (loginException: LoginsApiException) {
            Log.e(
                "Add new login",
                "Failed to add new login.",
                loginException,
            )
        }
        return encryptedLogin?.guid
    }

    // Create a [LoginEntry] for the edit login dialog
    private suspend fun loginEntryForSave(loginId: String, usernameText: String, passwordText: String): LoginEntry {
        // must retrieve from storage to get the httpsRealm and formActionOrigin
        val oldLogin = passwordsStorage.get(loginId)!!
        return LoginEntry(
            // Copied from the existing login
            origin = oldLogin.origin,
            httpRealm = oldLogin.httpRealm,
            formActionOrigin = oldLogin.formActionOrigin,
            // New values
            username = usernameText,
            password = passwordText,
        )
    }

    fun save(loginId: String, usernameText: String, passwordText: String) {
        var saveLoginJob: Deferred<Unit>? = null
        lifecycleScope.launch(ioDispatcher) {
            saveLoginJob = async {
                save(loginId, loginEntryForSave(loginId, usernameText, passwordText))
            }
            saveLoginJob?.await()
            withContext(Dispatchers.Main) {
                val directions =
                    EditLoginFragmentDirections.actionEditLoginFragmentToLoginDetailFragment(
                        loginId,
                    )
                navController.navigate(directions)
            }
        }
        saveLoginJob?.invokeOnCompletion {
            if (it is CancellationException) {
                saveLoginJob?.cancel()
            }
        }
    }

    private suspend fun save(guid: String, loginEntryToSave: LoginEntry) {
        try {
            val encryptedLogin = passwordsStorage.update(guid, loginEntryToSave)
            updateLoginInState(guid, passwordsStorage.decryptLogin(encryptedLogin))
        } catch (loginException: LoginsApiException) {
            when (loginException) {
                is NoSuchRecordException,
                is InvalidRecordException,
                -> {
                    Log.e(
                        "Edit login",
                        "Failed to save edited login.",
                        loginException,
                    )
                }
                else -> Log.e(
                    "Edit login",
                    "Failed to save edited login.",
                    loginException,
                )
            }
        }
    }

    private fun addLoginToState(updatedLogin: Login) {
        val login = updatedLogin.mapToSavedLogin()
        loginsFragmentStore.dispatch(
            LoginsAction.AddLogin(login),
        )
    }

    private fun updateLoginInState(loginId: String, updatedLogin: Login) {
        val login = updatedLogin.mapToSavedLogin()
        loginsFragmentStore.dispatch(
            LoginsAction.UpdateLogin(loginId, login),
        )
    }

    private fun deleteLoginFromState(loginId: String) {
        loginsFragmentStore.dispatch(
            LoginsAction.DeleteLogin(loginId),
        )
    }

    fun findDuplicateForAdd(originText: String, usernameText: String, passwordText: String) {
        lifecycleScope.launch(ioDispatcher) {
            findDuplicate(
                loginEntryForAdd(originText, usernameText, passwordText),
            )
        }
    }

    fun findDuplicateForSave(loginId: String, usernameText: String, passwordText: String) {
        lifecycleScope.launch(ioDispatcher) {
            findDuplicate(
                loginEntryForSave(loginId, usernameText, passwordText),
                loginId,
            )
        }
    }

    private suspend fun findDuplicate(entry: LoginEntry, currentGuid: String? = null) {
        // Ensure that we have a valid, non-blank password.  The value doesn't
        // matter for dupe-checking and we want to make sure that
        // findLoginToUpdate() doesn't throw an error because the [LoginEntry]
        // is invalid
        val validEntry = if (entry.password.isNotEmpty()) entry else entry.copy(password = "password")
        var dupe = try {
            passwordsStorage.findLoginToUpdate(validEntry)?.mapToSavedLogin()
        } catch (e: LoginsApiException) {
            // If the entry was invalid, then consider it not a dupe
            null
        }
        if (dupe != null && dupe.guid == currentGuid) {
            // If the found login matches the current login, don't consider it a dupe
            dupe = null
        }
        loginsFragmentStore.dispatch(LoginsAction.DuplicateLogin(dupe))
    }

    fun fetchLoginDetails(loginId: String) = lifecycleScope.launch(ioDispatcher) {
        val fetchedLogin = passwordsStorage.get(loginId)
        withContext(Dispatchers.Main) {
            if (fetchedLogin != null) {
                loginsFragmentStore.dispatch(
                    LoginsAction.UpdateCurrentLogin(
                        fetchedLogin.mapToSavedLogin(),
                    ),
                )
            } else {
                navController.popBackStack()
            }
        }
    }

    fun handleLoadAndMapLogins() {
        // Don't touch the store if we already have the logins loaded.
        // This has a slight downside of possibly being out of date with the storage if, say, Sync
        // ran in the meantime, but that's fairly unlikely and the speedy UI is worth it.
        if (loginsFragmentStore.state.loginList.isNotEmpty()) {
            lifecycleScope.launch(Dispatchers.Main) {
                loginsFragmentStore.dispatch(LoginsAction.LoginsListUpToDate)
            }
            return
        }
        var deferredLogins: Deferred<List<Login>>? = null
        val fetchLoginsJob = lifecycleScope.launch(ioDispatcher) {
            deferredLogins = async {
                passwordsStorage.list()
            }
            val logins = deferredLogins?.await()
            logins?.let {
                withContext(Dispatchers.Main) {
                    loginsFragmentStore.dispatch(
                        LoginsAction.UpdateLoginsList(
                            logins.map { it.mapToSavedLogin() },
                        ),
                    )
                }
            }
        }
        fetchLoginsJob.invokeOnCompletion {
            if (it is CancellationException) {
                deferredLogins?.cancel()
            }
        }
    }

    /**
     * Copy login username to clipboard
     * @param loginId id of the login entry to copy username from
     */
    fun copyUsername(loginId: String) = lifecycleScope.launch {
        val login = passwordsStorage.get(loginId)
        clipboardHandler.text = login?.username
    }

    /**
     * Copy login password to clipboard
     * @param loginId id of the login entry to copy password from
     */
    fun copyPassword(loginId: String) = lifecycleScope.launch {
        val login = passwordsStorage.get(loginId)
        clipboardHandler.sensitiveText = login?.password
    }
}
