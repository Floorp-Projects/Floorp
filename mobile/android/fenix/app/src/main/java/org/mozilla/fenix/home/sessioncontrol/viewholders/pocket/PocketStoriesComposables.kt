/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber")

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
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
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ClickableSubstringLink
import org.mozilla.fenix.compose.EagerFlingBehavior
import org.mozilla.fenix.compose.FakeClient
import org.mozilla.fenix.compose.ListItemTabLarge
import org.mozilla.fenix.compose.ListItemTabLargePlaceholder
import org.mozilla.fenix.compose.SelectableChip
import org.mozilla.fenix.compose.StaggeredHorizontalGrid
import org.mozilla.fenix.compose.TabSubtitle
import org.mozilla.fenix.compose.TabSubtitleWithInterdot
import org.mozilla.fenix.compose.TabTitle
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.roundToInt
import kotlin.random.Random

/**
 * Placeholder [PocketRecommendedStory] allowing to combine other items in the same list that shows stories.
 * It uses empty values for it's properties ensuring that no conflict is possible since real stories have
 * mandatory values.
 */
private val placeholderStory = PocketRecommendedStory("", "", "", "", "", 0, 0)

/**
 * Displays a single [PocketRecommendedStory].
 *
 * @param story The [PocketRecommendedStory] to be displayed.
 * @param client [Client] instance to be used for downloading the story header image.
 * @param onStoryClick Callback for when the user taps on this story.
 */
@Composable
fun PocketStory(
    @PreviewParameter(PocketStoryProvider::class) story: PocketRecommendedStory,
    client: Client,
    onStoryClick: (PocketRecommendedStory) -> Unit,
) {
    val imageUrl = story.imageUrl.replace(
        "{wh}",
        with(LocalDensity.current) { "${116.dp.toPx().roundToInt()}x${84.dp.toPx().roundToInt()}" }
    )
    val isValidPublisher = story.publisher.isNotBlank()
    val isValidTimeToRead = story.timeToRead >= 0
    ListItemTabLarge(
        client = client,
        imageUrl = imageUrl,
        onClick = { onStoryClick(story) },
        title = {
            TabTitle(text = story.title, maxLines = 3)
        },
        subtitle = {
            if (isValidPublisher && isValidTimeToRead) {
                TabSubtitleWithInterdot(story.publisher, "${story.timeToRead} min")
            } else if (isValidPublisher) {
                TabSubtitle(story.publisher)
            } else if (isValidTimeToRead) {
                TabSubtitle("${story.timeToRead} min")
            }
        }
    )
}

/**
 * Displays a list of [PocketRecommendedStory]es on 3 by 3 grid.
 * If there aren't enough stories to fill all columns placeholders containing an external link
 * to go to Pocket for more recommendations are added.
 *
 * @param stories The list of [PocketRecommendedStory]ies to be displayed. Expect a list with 8 items.
 * @param client [Client] instance to be used for downloading the story header image.
 * @param onExternalLinkClicked Callback for when the user taps an element which contains an
 * external link for where user can go for more recommendations.
 */
@Composable
fun PocketStories(
    @PreviewParameter(PocketStoryProvider::class) stories: List<PocketRecommendedStory>,
    client: Client,
    onExternalLinkClicked: (String) -> Unit
) {
    // Show stories in at most 3 rows but on any number of columns depending on the data received.
    val maxRowsNo = 3
    val storiesToShow = (stories + placeholderStory).chunked(maxRowsNo)

    val listState = rememberLazyListState()
    val flingBehavior = EagerFlingBehavior(lazyRowState = listState)

    LazyRow(
        contentPadding = PaddingValues(start = 8.dp, end = 8.dp),
        state = listState,
        flingBehavior = flingBehavior,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        itemsIndexed(storiesToShow) { _, columnItems ->
            Column {
                columnItems.forEachIndexed { rowIndex, story ->
                    if (story == placeholderStory) {
                        ListItemTabLargePlaceholder(stringResource(R.string.pocket_stories_placeholder_text)) {
                            onExternalLinkClicked("http://getpocket.com/explore")
                        }
                    } else {
                        PocketStory(story, client) {
                            onExternalLinkClicked(story.url)
                        }
                    }

                    if (rowIndex < maxRowsNo - 1) {
                        Spacer(modifier = Modifier.height(8.dp))
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
 * @param onCategoryClick Callback for when the user taps a category.
 */
@Composable
fun PocketStoriesCategories(
    categories: List<PocketRecommendedStoriesCategory>,
    selections: List<PocketRecommendedStoriesSelectedCategory>,
    onCategoryClick: (PocketRecommendedStoriesCategory) -> Unit
) {
    StaggeredHorizontalGrid(
        horizontalItemsSpacing = 16.dp
    ) {
        categories.filter { it.name != POCKET_STORIES_DEFAULT_CATEGORY_NAME }.forEach { category ->
            SelectableChip(category.name, selections.map { it.name }.contains(category.name)) {
                onCategoryClick(category)
            }
        }
    }
}

/**
 * Pocket feature section title.
 * Shows a default text about Pocket and offers a external link to learn more.
 *
 * @param onExternalLinkClicked Callback invoked when the user clicks the "Learn more" link.
 * Contains the full URL for where the user should be navigated to.
 */
@Composable
fun PoweredByPocketHeader(
    onExternalLinkClicked: (String) -> Unit,
) {
    val color = when (isSystemInDarkTheme()) {
        true -> PhotonColors.LightGrey30
        false -> PhotonColors.DarkGrey90
    }

    val link = stringResource(R.string.pocket_stories_feature_learn_more)
    val text = stringResource(R.string.pocket_stories_feature_caption, link)
    val linkStartIndex = text.indexOf(link)
    val linkEndIndex = linkStartIndex + link.length

    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Row(
            Modifier
                .fillMaxWidth()
                .semantics(mergeDescendants = true) { },
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
                    color = color,
                    fontSize = 12.sp,
                    lineHeight = 16.sp
                )

                ClickableSubstringLink(text, color, linkStartIndex, linkEndIndex) {
                    onExternalLinkClicked("https://www.mozilla.org/en-US/firefox/pocket/")
                }
            }
        }
    }
}

@Composable
@Preview
private fun PocketStoriesComposablesPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.surface)) {
            Column {
                PocketStories(
                    stories = getFakePocketStories(8),
                    client = FakeClient(),
                    onExternalLinkClicked = { }
                )
                Spacer(Modifier.height(10.dp))

                PocketStoriesCategories(
                    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor".split(" ").map {
                        PocketRecommendedStoriesCategory(it)
                    },
                    emptyList()
                ) { }
                Spacer(Modifier.height(10.dp))

                PoweredByPocketHeader { }
            }
        }
    }
}

private class PocketStoryProvider : PreviewParameterProvider<PocketRecommendedStory> {
    override val values = getFakePocketStories(7).asSequence()
    override val count = 8
}

private fun getFakePocketStories(limit: Int = 1): List<PocketRecommendedStory> {
    return mutableListOf<PocketRecommendedStory>().apply {
        for (index in 0 until limit) {
            val randomNumber = Random.nextInt(0, 10)

            add(
                PocketRecommendedStory(
                    title = "This is a ${"very ".repeat(randomNumber)} long title",
                    publisher = "Publisher",
                    url = "https://story$randomNumber.com",
                    imageUrl = "",
                    timeToRead = randomNumber,
                    category = "Category #$randomNumber",
                    timesShown = randomNumber.toLong()
                )
            )
        }
    }
}
