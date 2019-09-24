/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.os.Bundle
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import org.mozilla.fenix.R

class BookmarkDeselectNavigationListener(
    private val navController: NavController,
    private val viewModel: BookmarksSharedViewModel,
    private val bookmarkInteractor: BookmarkViewInteractor
) : NavController.OnDestinationChangedListener, LifecycleObserver {

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun onResume() {
        navController.addOnDestinationChangedListener(this)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun onDestroy() {
        navController.removeOnDestinationChangedListener(this)
    }

    /**
     * Deselects all items when the user navigates to a different fragment or a different folder.
     */
    override fun onDestinationChanged(controller: NavController, destination: NavDestination, arguments: Bundle?) {
        if (destination.id != R.id.bookmarkFragment || differentFromSelectedFolder(arguments)) {
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
