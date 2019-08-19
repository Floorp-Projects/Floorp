/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.Session
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.toolbar.QuickActionSheetAction
import java.net.MalformedURLException
import java.net.URL

class QuickActionSheetSessionObserver(
    private val parentScope: CoroutineScope,
    private val components: Components,
    private val dispatch: (QuickActionSheetAction) -> Unit
) : Session.Observer {

    private var findBookmarkJob: Job? = null

    override fun onLoadingStateChanged(session: Session, loading: Boolean) {
        if (!loading) {
            updateBookmarkState(session)
            dispatch(QuickActionSheetAction.BounceNeededChange)
        }
    }

    override fun onUrlChanged(session: Session, url: String) {
        updateBookmarkState(session)
        updateAppLinksState(session)
    }

    /**
     * Launches job to update the bookmark button on the quick action sheet.
     */
    fun updateBookmarkState(session: Session) {
        findBookmarkJob?.cancel()
        findBookmarkJob = parentScope.launch(Main) {
            val found = findBookmarkedURL(session)
            dispatch(QuickActionSheetAction.BookmarkedStateChange(found))
        }
    }

    /**
     * Updates the app link button on the quick action sheet.
     */
    private fun updateAppLinksState(session: Session) {
        val url = session.url
        val appLinks = components.useCases.appLinksUseCases.appLinkRedirect
        dispatch(QuickActionSheetAction.AppLinkStateChange(appLinks(url).hasExternalApp()))
    }

    private suspend fun findBookmarkedURL(session: Session): Boolean = withContext(IO) {
        try {
            val url = URL(session.url).toString()
            val list = components.core.bookmarksStorage.getBookmarksWithUrl(url)
            list.isNotEmpty() && list[0].url == url
        } catch (e: MalformedURLException) {
            false
        }
    }
}
