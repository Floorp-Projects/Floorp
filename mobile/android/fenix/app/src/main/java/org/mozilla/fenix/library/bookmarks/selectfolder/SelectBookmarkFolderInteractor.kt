/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.selectfolder

import android.content.Context
import androidx.navigation.NavController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel
import org.mozilla.fenix.library.bookmarks.SignInInteractor

class SelectBookmarkFolderInteractor(
    private val context: Context,
    private val navController: NavController,
    private val sharedViewModel: BookmarksSharedViewModel
) : SignInInteractor {

    override fun onSignInPressed() {
        context.components.services.launchPairingSignIn(context, navController)
    }

    override fun onSignedIn() {
        sharedViewModel.signedIn.postValue(true)
    }

    override fun onSignedOut() {
        sharedViewModel.signedIn.postValue(false)
    }
}
