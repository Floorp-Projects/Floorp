/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.os.Bundle
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import org.mozilla.fenix.R

class BookmarkDeselectNavigationListener(
    private val navController: NavController,
    private val viewModel: BookmarksSharedViewModel,
    private val bookmarkInteractor: BookmarkViewInteractor,
) : NavController.OnDestinationChangedListener, DefaultLifecycleObserver {

    override fun onResume(owner: LifecycleOwner) {
        navController.addOnDestinationChangedListener(this)
    }

    override fun onDestroy(owner: LifecycleOwner) {
        navController.removeOnDestinationChangedListener(this)
    }

    /**
     * Deselects all items when the user navigates to a different fragment or a different folder.
     */
    override fun onDestinationChanged(controller: NavController, destination: NavDestination, arguments: Bundle?) {
        if (destination.id != R.id.bookmarkFragment || differentFromSelectedFolder(arguments)) {
            // TODO this is currently called when opening the bookmark menu. Fix this if possible
            bookmarkInteractor.onAllBookmarksDeselected()
        }
    }

    /**
     * Returns true if the currentRoot listed in the [arguments] is different from the current selected folder.
     */
    private fun differentFromSelectedFolder(arguments: Bundle?): Boolean {
        return arguments != null &&
            BookmarkFragmentArgs.fromBundle(arguments).currentRoot != viewModel.selectedFolder?.guid
    }
}
