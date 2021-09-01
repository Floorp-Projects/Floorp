/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import android.view.View
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.compose.ui.unit.dp
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.home.HomeFragmentStore

private const val STORIES_TO_SHOW_COUNT = 7

/**
 * [RecyclerView.ViewHolder] that will display a list of [PocketRecommendedStory]es
 * which is to be provided in the [bind] method.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param store [HomeFragmentStore] containing the list of Pocket stories to be displayed.
 */
class PocketStoriesViewHolder(
    val composeView: ComposeView,
    val store: HomeFragmentStore,
    val client: Client
) : RecyclerView.ViewHolder(composeView) {

    init {
        composeView.setViewCompositionStrategy(
            ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed
        )
        composeView.setContent {
            PocketStories(store, client)
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}

@Composable
fun PocketStories(
    store: HomeFragmentStore,
    client: Client
) {
    val stories = store
        .observeAsComposableState { state -> state.pocketStories }.value
        ?.take(STORIES_TO_SHOW_COUNT)

    ExpandableCard(
        Modifier
            .fillMaxWidth()
            .padding(top = 40.dp)
    ) {
        PocketRecommendations {
            PocketStories(
                stories ?: emptyList(),
                client
            )
        }
    }
}
