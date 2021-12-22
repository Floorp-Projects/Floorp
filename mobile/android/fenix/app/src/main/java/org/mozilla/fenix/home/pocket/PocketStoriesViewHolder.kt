/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import android.view.View
import androidx.annotation.Dimension
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.compose.SectionHeader
import org.mozilla.fenix.home.HomeFragmentStore

internal const val POCKET_STORIES_TO_SHOW_COUNT = 8
internal const val POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT = 8

/**
 * [RecyclerView.ViewHolder] that will display a list of [PocketRecommendedStory]es
 * which is to be provided in the [bind] method.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param store [HomeFragmentStore] containing the list of Pocket stories to be displayed.
 * @param interactor [PocketStoriesInteractor] callback for user interaction.
 */
class PocketStoriesViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    val store: HomeFragmentStore,
    val interactor: PocketStoriesInteractor
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }

    @Composable
    override fun Content() {
        PocketStories(
            store,
            interactor::onStoriesShown,
            interactor::onStoryClicked,
            interactor::onCategoryClicked,
            interactor::onDiscoverMoreClicked,
            interactor::onLearnMoreClicked,
            with(composeView.resources) {
                getDimensionPixelSize(R.dimen.home_item_horizontal_margin) / displayMetrics.density
            }
        )
    }
}

@Composable
@Suppress("LongParameterList")
fun PocketStories(
    store: HomeFragmentStore,
    onStoriesShown: (List<PocketRecommendedStory>) -> Unit,
    onStoryClicked: (PocketRecommendedStory, Pair<Int, Int>) -> Unit,
    onCategoryClicked: (PocketRecommendedStoriesCategory) -> Unit,
    onDiscoverMoreClicked: (String) -> Unit,
    onLearnMoreClicked: (String) -> Unit,
    @Dimension horizontalPadding: Float = 0f
) {
    val stories = store
        .observeAsComposableState { state -> state.pocketStories }.value

    val categories = store
        .observeAsComposableState { state -> state.pocketStoriesCategories }.value

    val categoriesSelections = store
        .observeAsComposableState { state -> state.pocketStoriesCategoriesSelections }.value

    LaunchedEffect(stories) {
        // We should report back when a certain story is actually being displayed.
        // Cannot do it reliably so for now we'll just mass report everything as being displayed.
        stories?.let {
            onStoriesShown(it)
        }
    }

    Column(modifier = Modifier.padding(top = 72.dp)) {
        SectionHeader(
            text = stringResource(R.string.pocket_stories_header_1),
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = horizontalPadding.dp)
                .wrapContentHeight(align = Alignment.Top)
        )

        Spacer(Modifier.height(17.dp))

        PocketStories(
            stories ?: emptyList(),
            horizontalPadding.dp,
            onStoryClicked,
            onDiscoverMoreClicked
        )

        Spacer(Modifier.height(24.dp))

        SectionHeader(
            text = stringResource(R.string.pocket_stories_categories_header),
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = horizontalPadding.dp)
                .wrapContentHeight(align = Alignment.Top)
        )

        Spacer(Modifier.height(17.dp))

        PocketStoriesCategories(
            categories = categories ?: emptyList(),
            selections = categoriesSelections ?: emptyList(),
            onCategoryClick = onCategoryClicked,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = horizontalPadding.dp)
        )

        Spacer(Modifier.height(24.dp))

        PoweredByPocketHeader(
            onLearnMoreClicked,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = horizontalPadding.dp)
        )
    }
}
