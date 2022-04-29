/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber")

package org.mozilla.fenix.home.pocket

import android.net.Uri
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryShim
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ClickableSubstringLink
import org.mozilla.fenix.compose.EagerFlingBehavior
import org.mozilla.fenix.compose.ListItemTabLarge
import org.mozilla.fenix.compose.ListItemTabLargePlaceholder
import org.mozilla.fenix.compose.ListItemTabSurface
import org.mozilla.fenix.compose.SelectableChip
import org.mozilla.fenix.compose.StaggeredHorizontalGrid
import org.mozilla.fenix.compose.PrimaryText
import org.mozilla.fenix.compose.TabSubtitleWithInterdot
import org.mozilla.fenix.compose.SecondaryText
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import kotlin.math.roundToInt

private const val URI_PARAM_UTM_KEY = "utm_source"
private const val POCKET_STORIES_UTM_VALUE = "pocket-newtab-android"
private const val POCKET_FEATURE_UTM_KEY_VALUE = "utm_source=ff_android"

/**
 * Placeholder [PocketStory] allowing to combine other items in the same list that shows stories.
 * It uses empty values for it's properties ensuring that no conflict is possible since real stories have
 * mandatory values.
 */
private val placeholderStory = PocketRecommendedStory("", "", "", "", "", 0, 0)

/**
 * Displays a single [PocketRecommendedStory].
 *
 * @param story The [PocketRecommendedStory] to be displayed.
 * @param onStoryClick Callback for when the user taps on this story.
 */
@Composable
fun PocketStory(
    @PreviewParameter(PocketStoryProvider::class) story: PocketRecommendedStory,
    onStoryClick: (PocketRecommendedStory) -> Unit,
) {
    val imageUrl = story.imageUrl.replace(
        "{wh}",
        with(LocalDensity.current) { "${116.dp.toPx().roundToInt()}x${84.dp.toPx().roundToInt()}" }
    )
    val isValidPublisher = story.publisher.isNotBlank()
    val isValidTimeToRead = story.timeToRead >= 0
    ListItemTabLarge(
        imageUrl = imageUrl,
        onClick = { onStoryClick(story) },
        title = {
            PrimaryText(
                text = story.title,
                fontSize = 14.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 2,
            )
        },
        subtitle = {
            if (isValidPublisher && isValidTimeToRead) {
                TabSubtitleWithInterdot(story.publisher, "${story.timeToRead} min")
            } else if (isValidPublisher) {
                SecondaryText(
                    text = story.publisher,
                    fontSize = 12.sp,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 1,
                )
            } else if (isValidTimeToRead) {
                SecondaryText(
                    text = "${story.timeToRead} min",
                    fontSize = 12.sp,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 1,
                )
            }
        }
    )
}

/**
 * Displays a single [PocketSponsoredStory].
 *
 * @param story The [PocketSponsoredStory] to be displayed.
 * @param onStoryClick Callback for when the user taps on this story.
 */
@Composable
fun PocketSponsoredStory(
    story: PocketSponsoredStory,
    onStoryClick: (PocketSponsoredStory) -> Unit
) {
    val (imageWidth, imageHeight) = with(LocalDensity.current) {
        116.dp.toPx().roundToInt() to 84.dp.toPx().roundToInt()
    }
    val imageUrl = story.imageUrl.replace(
        "&resize=w[0-9]+-h[0-9]+".toRegex(),
        "&resize=w$imageWidth-h$imageHeight"
    )

    ListItemTabSurface(imageUrl, { onStoryClick(story) }) {
        PrimaryText(
            text = story.title,
            fontSize = 14.sp,
            overflow = TextOverflow.Ellipsis,
            maxLines = 2,
        )

        Spacer(Modifier.height(9.dp))

        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                painter = painterResource(id = R.drawable.pocket_star_stroke),
                contentDescription = null,
                tint = FirefoxTheme.colors.iconSecondary,
            )

            Spacer(Modifier.width(8.dp))

            SecondaryText(
                text = stringResource(R.string.pocket_stories_sponsor_indication),
                fontSize = 12.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )
        }

        Spacer(Modifier.height(7.dp))

        SecondaryText(
            text = story.sponsor,
            fontSize = 12.sp,
            overflow = TextOverflow.Ellipsis,
            maxLines = 1,
        )
    }
}

/**
 * Displays a list of [PocketStory]es on 3 by 3 grid.
 * If there aren't enough stories to fill all columns placeholders containing an external link
 * to go to Pocket for more recommendations are added.
 *
 * @param stories The list of [PocketStory]ies to be displayed. Expect a list with 8 items.
 * @param contentPadding Dimension for padding the content after it has been clipped.
 * This space will be used for shadows and also content rendering when the list is scrolled.
 * @param onStoryClicked Callback for when the user taps on a recommended story.
 * @param onDiscoverMoreClicked Callback for when the user taps an element which contains an
 */
@Composable
fun PocketStories(
    @PreviewParameter(PocketStoryProvider::class) stories: List<PocketStory>,
    contentPadding: Dp,
    onStoryClicked: (PocketStory, Pair<Int, Int>) -> Unit,
    onDiscoverMoreClicked: (String) -> Unit
) {
    // Show stories in at most 3 rows but on any number of columns depending on the data received.
    val maxRowsNo = 3
    val storiesToShow = (stories + placeholderStory).chunked(maxRowsNo)

    val listState = rememberLazyListState()
    val flingBehavior = EagerFlingBehavior(lazyRowState = listState)

    LazyRow(
        contentPadding = PaddingValues(horizontal = contentPadding),
        state = listState,
        flingBehavior = flingBehavior,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        itemsIndexed(storiesToShow) { columnIndex, columnItems ->
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                columnItems.forEachIndexed { rowIndex, story ->
                    if (story == placeholderStory) {
                        ListItemTabLargePlaceholder(stringResource(R.string.pocket_stories_placeholder_text)) {
                            onDiscoverMoreClicked("https://getpocket.com/explore?$POCKET_FEATURE_UTM_KEY_VALUE")
                        }
                    } else if (story is PocketRecommendedStory) {
                        PocketStory(story) {
                            val uri = Uri.parse(story.url)
                                .buildUpon()
                                .appendQueryParameter(URI_PARAM_UTM_KEY, POCKET_STORIES_UTM_VALUE)
                                .build().toString()
                            onStoryClicked(it.copy(url = uri), rowIndex to columnIndex)
                        }
                    } else if (story is PocketSponsoredStory) {
                        PocketSponsoredStory(story) {
                            onStoryClicked(story, rowIndex to columnIndex)
                        }
                    }
                }
            }
        }
    }
}

/**
 * Displays a list of [PocketRecommendedStoriesCategory]s.
 *
 * @param categories The categories needed to be displayed.
 * @param selections List of categories currently selected.
 * @param onCategoryClick Callback for when the user taps a category.
 * @param modifier [Modifier] to be applied to the layout.
 */
@Composable
fun PocketStoriesCategories(
    categories: List<PocketRecommendedStoriesCategory>,
    selections: List<PocketRecommendedStoriesSelectedCategory>,
    onCategoryClick: (PocketRecommendedStoriesCategory) -> Unit,
    modifier: Modifier = Modifier
) {
    Box(modifier = modifier) {
        StaggeredHorizontalGrid(
            horizontalItemsSpacing = 16.dp,
            verticalItemsSpacing = 16.dp
        ) {
            categories.filter { it.name != POCKET_STORIES_DEFAULT_CATEGORY_NAME }.forEach { category ->
                SelectableChip(category.name, selections.map { it.name }.contains(category.name)) {
                    onCategoryClick(category)
                }
            }
        }
    }
}

/**
 * Pocket feature section title.
 * Shows a default text about Pocket and offers a external link to learn more.
 *
 * @param onLearnMoreClicked Callback invoked when the user clicks the "Learn more" link.
 * Contains the full URL for where the user should be navigated to.
 * @param modifier [Modifier] to be applied to the layout.
 */
@Composable
fun PoweredByPocketHeader(
    onLearnMoreClicked: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    val link = stringResource(R.string.pocket_stories_feature_learn_more)
    val text = stringResource(R.string.pocket_stories_feature_caption, link)
    val linkStartIndex = text.indexOf(link)
    val linkEndIndex = linkStartIndex + link.length

    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Row(
            Modifier
                .fillMaxWidth()
                .semantics(mergeDescendants = true) {},
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                painter = painterResource(id = R.drawable.pocket_vector),
                contentDescription = null,
                // Apply the red tint in code. Otherwise the image is black and white.
                tint = Color(0xFFEF4056)
            )

            Spacer(modifier = Modifier.width(16.dp))

            Column {
                Text(
                    text = stringResource(R.string.pocket_stories_feature_title),
                    color = FirefoxTheme.colors.textPrimary,
                    fontSize = 12.sp,
                    lineHeight = 16.sp
                )

                ClickableSubstringLink(
                    text = text,
                    textColor = FirefoxTheme.colors.textPrimary,
                    clickableStartIndex = linkStartIndex,
                    clickableEndIndex = linkEndIndex
                ) {
                    onLearnMoreClicked("https://www.mozilla.org/en-US/firefox/pocket/?$POCKET_FEATURE_UTM_KEY_VALUE")
                }
            }
        }
    }
}

@Composable
@Preview
private fun PocketStoriesComposablesPreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Box(Modifier.background(FirefoxTheme.colors.layer2)) {
            Column {
                PocketStories(
                    stories = getFakePocketStories(8),
                    contentPadding = 0.dp,
                    onStoryClicked = { _, _ -> },
                    onDiscoverMoreClicked = {}
                )
                Spacer(Modifier.height(10.dp))

                PocketStoriesCategories(
                    categories = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor"
                        .split(" ")
                        .map { PocketRecommendedStoriesCategory(it) },
                    selections = emptyList(),
                    onCategoryClick = {}
                )
                Spacer(Modifier.height(10.dp))

                PoweredByPocketHeader(
                    onLearnMoreClicked = {}
                )
            }
        }
    }
}

private class PocketStoryProvider : PreviewParameterProvider<PocketStory> {
    override val values = getFakePocketStories(7).asSequence()
    override val count = 8
}

internal fun getFakePocketStories(limit: Int = 1): List<PocketStory> {
    return mutableListOf<PocketStory>().apply {
        for (index in 0 until limit) {
            when (index % 2 == 0) {
                true -> add(
                    PocketRecommendedStory(
                        title = "This is a ${"very ".repeat(index)} long title",
                        publisher = "Publisher",
                        url = "https://story$index.com",
                        imageUrl = "",
                        timeToRead = index,
                        category = "Category #$index",
                        timesShown = index.toLong()
                    )
                )
                false -> add(
                    PocketSponsoredStory(
                        title = "This is a ${"very ".repeat(index)} long title",
                        url = "https://sponsored-story$index.com",
                        imageUrl = "",
                        sponsor = "Mozilla",
                        shim = PocketSponsoredStoryShim("", "")
                    )
                )
            }
        }
    }
}
