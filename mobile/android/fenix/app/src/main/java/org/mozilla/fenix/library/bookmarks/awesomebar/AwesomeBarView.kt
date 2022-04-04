/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.awesomebar

import androidx.appcompat.content.res.AppCompatResources.getDrawable
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.session.SessionUseCases
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.library.bookmarks.BookmarkSearchFragmentState

/**
 * View that contains and configures the BrowserAwesomeBar
 */
class AwesomeBarView(
    activity: HomeActivity,
    val interactor: AwesomeBarInteractor,
    val view: AwesomeBarWrapper,
) {
    private val bookmarksStorageSuggestionProvider: BookmarksStorageSuggestionProvider

    private val loadUrlUseCase = object : SessionUseCases.LoadUrlUseCase {
        override fun invoke(
            url: String,
            flags: EngineSession.LoadUrlFlags,
            additionalHeaders: Map<String, String>?
        ) {
            interactor.onUrlTapped(url, flags)
        }
    }

    init {
        val components = activity.components

        val engineForSpeculativeConnects = when (activity.browsingModeManager.mode) {
            BrowsingMode.Normal -> components.core.engine
            BrowsingMode.Private -> null
        }

        bookmarksStorageSuggestionProvider =
            BookmarksStorageSuggestionProvider(
                bookmarksStorage = components.core.bookmarksStorage,
                loadUrlUseCase = loadUrlUseCase,
                icons = components.core.icons,
                indicatorIcon = getDrawable(activity, R.drawable.ic_search_results_bookmarks),
                engine = engineForSpeculativeConnects,
                showEditSuggestion = false,
            )

        view.addProviders(bookmarksStorageSuggestionProvider)
    }

    fun update(state: BookmarkSearchFragmentState) {
        view.onInputChanged(state.query)
    }
}
