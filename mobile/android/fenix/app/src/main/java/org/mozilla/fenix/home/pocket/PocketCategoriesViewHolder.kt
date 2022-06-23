/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import android.view.View
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.compose.SectionHeader
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

internal const val POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT = 8

/**
 * [RecyclerView.ViewHolder] for displaying the list of [PocketRecommendedStoriesCategory]s from
 * [AppStore].
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 * @param interactor [PocketStoriesInteractor] callback for user interaction.
 */
class PocketCategoriesViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: PocketStoriesInteractor
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    @Composable
    override fun Content() {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)

        val homeScreenReady = components.appStore
            .observeAsComposableState { state -> state.firstFrameDrawn }.value ?: false

        val categories = components.appStore
            .observeAsComposableState { state -> state.pocketStoriesCategories }.value
        val categoriesSelections = components.appStore
            .observeAsComposableState { state -> state.pocketStoriesCategoriesSelections }.value

        // See the detailed comment in PocketStoriesViewHolder for reasoning behind this change.
        if (!homeScreenReady) return
        Column {
            Spacer(Modifier.height(24.dp))

            PocketTopics(
                categories = categories ?: emptyList(),
                categoriesSelections = categoriesSelections ?: emptyList(),
                onCategoryClick = interactor::onCategoryClicked
            )
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}

@Composable
private fun PocketTopics(
    categories: List<PocketRecommendedStoriesCategory> = emptyList(),
    categoriesSelections: List<PocketRecommendedStoriesSelectedCategory> = emptyList(),
    onCategoryClick: (PocketRecommendedStoriesCategory) -> Unit
) {
    Column {
        SectionHeader(
            text = stringResource(R.string.pocket_stories_categories_header),
            modifier = Modifier
                .fillMaxWidth()
                .wrapContentHeight(align = Alignment.Top)
        )

        Spacer(Modifier.height(16.dp))

        PocketStoriesCategories(
            categories = categories,
            selections = categoriesSelections,
            onCategoryClick = onCategoryClick,
            modifier = Modifier
                .fillMaxWidth()
        )
    }
}

@Composable
@Preview
private fun PocketCategoriesViewHolderPreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        PocketTopics(
            categories = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor"
                .split(" ")
                .map { PocketRecommendedStoriesCategory(it) },
            categoriesSelections = emptyList(),
            onCategoryClick = {}
        )
    }
}
