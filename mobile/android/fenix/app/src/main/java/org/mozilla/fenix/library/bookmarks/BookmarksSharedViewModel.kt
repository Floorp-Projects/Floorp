/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager

/**
 * [ViewModel] that shares data between various bookmarks fragments.
 */
class BookmarksSharedViewModel : ViewModel(), AccountObserver {

    private val signedInMutable = MutableLiveData(true)

    /**
     * Whether or not the user is signed in.
     */
    val signedIn: LiveData<Boolean> get() = signedInMutable

    /**
     * The currently selected bookmark root.
     */
    var selectedFolder: BookmarkNode? = null

    /**
     * Updates the [signedIn] boolean once the account observer sees that the user logged in.
     */
    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        signedInMutable.postValue(true)
    }

    /**
     * Updates the [signedIn] boolean once the account observer sees that the user logged out.
     */
    override fun onLoggedOut() {
        signedInMutable.postValue(false)
    }

    fun observeAccountManager(accountManager: FxaAccountManager, owner: LifecycleOwner) {
        accountManager.register(this, owner = owner)
        if (accountManager.authenticatedAccount() != null) {
            signedInMutable.postValue(true)
        } else {
            signedInMutable.postValue(false)
        }
    }
}
