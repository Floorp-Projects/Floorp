/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.home.HomeFragmentStore

/**
 * [RecyclerView.ViewHolder] that will display a list of [PocketRecommendedStory]es
 * which is to be provided in the [bind] method.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param store [HomeFragmentStore] containing the list of Pocket stories to be displayed.
 */
class PocketStoriesViewHolder(
    val composeView: ComposeView,
    val store: HomeFragmentStore
) : RecyclerView.ViewHolder(composeView) {

    init {
        composeView.setViewCompositionStrategy(
            ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed
        )
        composeView.setContent {
            PocketStories(store)
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}

@Composable
fun PocketStories(store: HomeFragmentStore) {
    val stories = store.observeAsComposableState { state -> state.pocketArticles }

    ExpandableCard {
        PocketRecommendations {
            PocketStories(stories.value ?: emptyList())
        }
    }
}
