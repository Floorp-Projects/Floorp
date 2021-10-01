/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import android.view.View
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.HomeSectionHeader
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.theme.FirefoxTheme

internal const val POCKET_STORIES_TO_SHOW_COUNT = 8
internal const val POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT = 8

/**
 * [RecyclerView.ViewHolder] that will display a list of [PocketRecommendedStory]es
 * which is to be provided in the [bind] method.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param store [HomeFragmentStore] containing the list of Pocket stories to be displayed.
 * @param client [Client] instance used for the stories header images.
 * @param interactor [PocketStoriesInteractor] callback for user interaction.
 */
class PocketStoriesViewHolder(
    val composeView: ComposeView,
    val store: HomeFragmentStore,
    val client: Client,
    val interactor: PocketStoriesInteractor
) : RecyclerView.ViewHolder(composeView) {

    init {
        composeView.setViewCompositionStrategy(
            ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed
        )
        composeView.setContent {
            FirefoxTheme {
                PocketStories(
                    store,
                    client,
                    interactor::onStoriesShown,
                    interactor::onCategoryClick,
                    interactor::onExternalLinkClicked
                )
            }
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}

@Composable
fun PocketStories(
    store: HomeFragmentStore,
    client: Client,
    onStoriesShown: (List<PocketRecommendedStory>) -> Unit,
    onCategoryClick: (PocketRecommendedStoryCategory) -> Unit,
    onExternalLinkClicked: (String) -> Unit
) {
    val stories = store
        .observeAsComposableState { state -> state.pocketStories }.value

    val categories = store
        .observeAsComposableState { state -> state.pocketStoriesCategories }.value

    LaunchedEffect(stories) {
        // We should report back when a certain story is actually being displayed.
        // Cannot do it reliably so for now we'll just mass report everything as being displayed.
        stories?.let {
            onStoriesShown(it)
        }
    }

    Column(modifier = Modifier.padding(vertical = 48.dp)) {
        HomeSectionHeader(
            text = stringResource(R.string.pocket_stories_header_1),
            modifier = Modifier
                .fillMaxWidth()
        )

        Spacer(Modifier.height(17.dp))

        PocketStories(stories ?: emptyList(), client, onExternalLinkClicked)

        Spacer(Modifier.height(24.dp))

        HomeSectionHeader(
            text = stringResource(R.string.pocket_stories_categories_header),
            modifier = Modifier.fillMaxWidth()
        )

        Spacer(Modifier.height(17.dp))

        PocketStoriesCategories(categories ?: emptyList()) {
            onCategoryClick(it)
        }

        Spacer(Modifier.height(24.dp))

        PoweredByPocketHeader(onExternalLinkClicked)
    }
}
