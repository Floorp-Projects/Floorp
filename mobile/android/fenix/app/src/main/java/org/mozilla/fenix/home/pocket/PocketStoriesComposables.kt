/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber")

package org.mozilla.fenix.home.pocket

import android.content.res.Configuration
import android.graphics.Rect
import android.net.Uri
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.onClick
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.max
import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryCaps
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryShim
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ClickableSubstringLink
import org.mozilla.fenix.compose.EagerFlingBehavior
import org.mozilla.fenix.compose.ITEM_WIDTH
import org.mozilla.fenix.compose.ListItemTabLarge
import org.mozilla.fenix.compose.ListItemTabLargePlaceholder
import org.mozilla.fenix.compose.ListItemTabSurface
import org.mozilla.fenix.compose.SelectableChip
import org.mozilla.fenix.compose.SelectableChipColors
import org.mozilla.fenix.compose.StaggeredHorizontalGrid
import org.mozilla.fenix.compose.TabSubtitleWithInterdot
import org.mozilla.fenix.compose.ext.onShown
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme
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
 * @param backgroundColor The background [Color] of the story.
 * @param onStoryClick Callback for when the user taps on this story.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Composable
fun PocketStory(
    @PreviewParameter(PocketStoryProvider::class) story: PocketRecommendedStory,
    backgroundColor: Color,
    onStoryClick: (PocketRecommendedStory) -> Unit,
) {
    val imageUrl = story.imageUrl.replace(
        "{wh}",
        with(LocalDensity.current) { "${116.dp.toPx().roundToInt()}x${84.dp.toPx().roundToInt()}" },
    )
    val isValidPublisher = story.publisher.isNotBlank()
    val isValidTimeToRead = story.timeToRead >= 0
    ListItemTabLarge(
        imageUrl = imageUrl,
        backgroundColor = backgroundColor,
        onClick = { onStoryClick(story) },
        title = {
            Text(
                text = story.title,
                modifier = Modifier.semantics {
                    testTagsAsResourceId = true
                    testTag = "pocket.story.title"
                },
                color = FirefoxTheme.colors.textPrimary,
                overflow = TextOverflow.Ellipsis,
                maxLines = 2,
                style = FirefoxTheme.typography.body2,
            )
        },
        subtitle = {
            if (isValidPublisher && isValidTimeToRead) {
                TabSubtitleWithInterdot(story.publisher, "${story.timeToRead} min")
            } else if (isValidPublisher) {
                Text(
                    text = story.publisher,
                    modifier = Modifier.semantics {
                        testTagsAsResourceId = true
                        testTag = "pocket.story.publisher"
                    },
                    color = FirefoxTheme.colors.textSecondary,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 1,
                    style = FirefoxTheme.typography.caption,
                )
            } else if (isValidTimeToRead) {
                Text(
                    text = "${story.timeToRead} min",
                    modifier = Modifier.semantics {
                        testTagsAsResourceId = true
                        testTag = "pocket.story.timeToRead"
                    },
                    color = FirefoxTheme.colors.textSecondary,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 1,
                    style = FirefoxTheme.typography.caption,
                )
            }
        },
    )
}

/**
 * Displays a single [PocketSponsoredStory].
 *
 * @param story The [PocketSponsoredStory] to be displayed.
 * @param backgroundColor The background [Color] of the story.
 * @param onStoryClick Callback for when the user taps on this story.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Composable
fun PocketSponsoredStory(
    story: PocketSponsoredStory,
    backgroundColor: Color,
    onStoryClick: (PocketSponsoredStory) -> Unit,
) {
    val (imageWidth, imageHeight) = with(LocalDensity.current) {
        116.dp.toPx().roundToInt() to 84.dp.toPx().roundToInt()
    }
    val imageUrl = story.imageUrl.replace(
        "&resize=w[0-9]+-h[0-9]+".toRegex(),
        "&resize=w$imageWidth-h$imageHeight",
    )

    ListItemTabSurface(
        imageUrl = imageUrl,
        contentPadding = PaddingValues(16.dp, 0.dp),
        backgroundColor = backgroundColor,
        onClick = { onStoryClick(story) },
    ) {
        Column(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.SpaceEvenly,
        ) {
            Text(
                text = story.title,
                modifier = Modifier.semantics {
                    testTagsAsResourceId = true
                    testTag = "pocket.sponsoredStory.title"
                },
                color = FirefoxTheme.colors.textPrimary,
                overflow = TextOverflow.Ellipsis,
                maxLines = 2,
                style = FirefoxTheme.typography.body2,
            )

            Text(
                text = stringResource(R.string.pocket_stories_sponsor_indication),
                modifier = Modifier.semantics {
                    testTagsAsResourceId = true
                    testTag = "pocket.sponsoredStory.identifier"
                },
                color = FirefoxTheme.colors.textSecondary,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
                style = FirefoxTheme.typography.caption,
            )

            Text(
                text = story.sponsor,
                modifier = Modifier.semantics {
                    testTagsAsResourceId = true
                    testTag = "pocket.sponsoredStory.sponsor"
                },
                color = FirefoxTheme.colors.textSecondary,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
                style = FirefoxTheme.typography.caption,
            )
        }
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
 * @param backgroundColor The background [Color] of each story.
 * @param onStoryShown Callback for when a certain story is visible to the user.
 * @param onStoryClicked Callback for when the user taps on a recommended story.
 * @param onDiscoverMoreClicked Callback for when the user taps an element which contains an
 */
@OptIn(ExperimentalComposeUiApi::class)
@Suppress("LongParameterList", "LongMethod")
@Composable
fun PocketStories(
    @PreviewParameter(PocketStoryProvider::class) stories: List<PocketStory>,
    contentPadding: Dp,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    onStoryShown: (PocketStory, Pair<Int, Int>) -> Unit,
    onStoryClicked: (PocketStory, Pair<Int, Int>) -> Unit,
    onDiscoverMoreClicked: (String) -> Unit,
) {
    // Show stories in at most 3 rows but on any number of columns depending on the data received.
    val maxRowsNo = 3
    val storiesToShow = (stories + placeholderStory).chunked(maxRowsNo)

    val listState = rememberLazyListState()
    val flingBehavior = EagerFlingBehavior(lazyRowState = listState)

    val configuration = LocalConfiguration.current
    val screenWidth = configuration.screenWidthDp.dp

    val endPadding =
        remember { mutableStateOf(endPadding(configuration, screenWidth, contentPadding)) }
    // Force recomposition as padding is not consistently updated when orientation has changed.
    endPadding.value = endPadding(configuration, screenWidth, contentPadding)

    LazyRow(
        modifier = Modifier.semantics {
            testTagsAsResourceId = true
            testTag = "pocket.stories"
        },
        contentPadding = PaddingValues(start = contentPadding, end = endPadding.value),
        state = listState,
        flingBehavior = flingBehavior,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        itemsIndexed(storiesToShow) { columnIndex, columnItems ->
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                columnItems.forEachIndexed { rowIndex, story ->
                    Box(
                        modifier = Modifier.semantics {
                            testTagsAsResourceId = true
                            testTag = when (story) {
                                placeholderStory -> "pocket.discover.more.story"
                                is PocketRecommendedStory -> "pocket.recommended.story"
                                else -> "pocket.sponsored.story"
                            }
                        },
                    ) {
                        if (story == placeholderStory) {
                            ListItemTabLargePlaceholder(
                                text = stringResource(R.string.pocket_stories_placeholder_text),
                                backgroundColor = backgroundColor,
                            ) {
                                onDiscoverMoreClicked("https://getpocket.com/explore?$POCKET_FEATURE_UTM_KEY_VALUE")
                            }
                        } else if (story is PocketRecommendedStory) {
                            PocketStory(
                                story = story,
                                backgroundColor = backgroundColor,
                            ) {
                                val uri = Uri.parse(story.url)
                                    .buildUpon()
                                    .appendQueryParameter(URI_PARAM_UTM_KEY, POCKET_STORIES_UTM_VALUE)
                                    .build().toString()
                                onStoryClicked(it.copy(url = uri), rowIndex to columnIndex)
                            }
                        } else if (story is PocketSponsoredStory) {
                            val screenBounds = Rect()
                                .apply { LocalView.current.getWindowVisibleDisplayFrame(this) }
                                .apply {
                                    // Check if this is in a preview because `.settings()` breaks previews
                                    if (!inComposePreview) {
                                        val verticalOffset = LocalContext.current.resources.getDimensionPixelSize(
                                            R.dimen.browser_toolbar_height,
                                        )

                                        if (LocalContext.current.settings().shouldUseBottomToolbar) {
                                            bottom -= verticalOffset
                                        } else {
                                            top += verticalOffset
                                        }
                                    }
                                }
                            Box(
                                modifier = Modifier.onShown(
                                    threshold = 0.5f,
                                    onVisible = { onStoryShown(story, rowIndex to columnIndex) },
                                    screenBounds = screenBounds,
                                ),
                            ) {
                                PocketSponsoredStory(
                                    story = story,
                                    backgroundColor = backgroundColor,
                                ) {
                                    onStoryClicked(story, rowIndex to columnIndex)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

private fun endPadding(configuration: Configuration, screenWidth: Dp, contentPadding: Dp) =
    if (configuration.orientation == Configuration.ORIENTATION_PORTRAIT) {
        alignColumnToTitlePadding(screenWidth = screenWidth, contentPadding = contentPadding)
    } else {
        contentPadding
    }

/**
 * If the column item is wider than the [screenWidth] default to the [contentPadding].
 */
private fun alignColumnToTitlePadding(screenWidth: Dp, contentPadding: Dp) =
    max(screenWidth - (ITEM_WIDTH.dp + contentPadding), contentPadding)

/**
 * Displays a list of [PocketRecommendedStoriesCategory]s.
 *
 * @param categories The categories needed to be displayed.
 * @param selections List of categories currently selected.
 * @param modifier [Modifier] to be applied to the layout.
 * @param categoryColors The color set defined by [SelectableChipColors] used to style Pocket categories.
 * @param onCategoryClick Callback for when the user taps a category.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Suppress("LongParameterList")
@Composable
fun PocketStoriesCategories(
    categories: List<PocketRecommendedStoriesCategory>,
    selections: List<PocketRecommendedStoriesSelectedCategory>,
    modifier: Modifier = Modifier,
    categoryColors: SelectableChipColors = SelectableChipColors.buildColors(),
    onCategoryClick: (PocketRecommendedStoriesCategory) -> Unit,
) {
    Box(
        modifier = modifier.semantics {
            testTagsAsResourceId = true
            testTag = "pocket.categories"
        },
    ) {
        StaggeredHorizontalGrid(
            horizontalItemsSpacing = 16.dp,
            verticalItemsSpacing = 16.dp,
        ) {
            categories.filter { it.name != POCKET_STORIES_DEFAULT_CATEGORY_NAME }.forEach { category ->
                SelectableChip(
                    text = category.name,
                    isSelected = selections.map { it.name }.contains(category.name),
                    isSquare = true,
                    selectableChipColors = categoryColors,
                ) {
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
 * @param textColor [Color] to be applied to the text.
 * @param linkTextColor [Color] of the link text.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Suppress("Deprecation")
@Composable
fun PoweredByPocketHeader(
    onLearnMoreClicked: (String) -> Unit,
    modifier: Modifier = Modifier,
    textColor: Color = FirefoxTheme.colors.textPrimary,
    linkTextColor: Color = FirefoxTheme.colors.textAccent,
) {
    val link = stringResource(R.string.pocket_stories_feature_learn_more)
    val text = stringResource(R.string.pocket_stories_feature_caption, link)
    val linkStartIndex = text.indexOf(link)
    val linkEndIndex = linkStartIndex + link.length

    Column(
        modifier = modifier.semantics {
            testTagsAsResourceId = true
            testTag = "pocket.header"
        },
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Row(
            Modifier
                .fillMaxWidth()
                .semantics(mergeDescendants = true) {},
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                painter = painterResource(id = R.drawable.pocket_vector),
                contentDescription = null,
                // Apply the red tint in code. Otherwise the image is black and white.
                tint = Color(0xFFEF4056),
            )

            Spacer(modifier = Modifier.width(16.dp))

            val onClickLabel = stringResource(id = R.string.a11y_action_label_pocket_learn_more)
            Column(
                Modifier.semantics(mergeDescendants = true) {
                    role = Role.Button
                    onClick(label = onClickLabel) {
                        onLearnMoreClicked(
                            "https://www.mozilla.org/en-US/firefox/pocket/?$POCKET_FEATURE_UTM_KEY_VALUE",
                        )
                        false
                    }
                },
            ) {
                Text(
                    text = stringResource(
                        R.string.pocket_stories_feature_title_2,
                        LocalContext.current.getString(R.string.pocket_product_name),
                    ),
                    modifier = Modifier.semantics {
                        testTagsAsResourceId = true
                        testTag = "pocket.header.title"
                    },
                    color = textColor,
                    style = FirefoxTheme.typography.caption,
                )

                Box(
                    modifier = modifier.semantics {
                        testTagsAsResourceId = true
                        testTag = "pocket.header.subtitle"
                    },
                ) {
                    ClickableSubstringLink(
                        text = text,
                        textColor = textColor,
                        linkTextColor = linkTextColor,
                        linkTextDecoration = TextDecoration.Underline,
                        clickableStartIndex = linkStartIndex,
                        clickableEndIndex = linkEndIndex,
                    ) {
                        onLearnMoreClicked(
                            "https://www.mozilla.org/en-US/firefox/pocket/?$POCKET_FEATURE_UTM_KEY_VALUE",
                        )
                    }
                }
            }
        }
    }
}

@Composable
@Preview
private fun PocketStoriesComposablesPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer2)) {
            Column {
                PocketStories(
                    stories = getFakePocketStories(8),
                    contentPadding = 0.dp,
                    onStoryShown = { _, _ -> },
                    onStoryClicked = { _, _ -> },
                    onDiscoverMoreClicked = {},
                )
                Spacer(Modifier.height(10.dp))

                PocketStoriesCategories(
                    categories = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor"
                        .split(" ")
                        .map { PocketRecommendedStoriesCategory(it) },
                    selections = emptyList(),
                    onCategoryClick = {},
                )
                Spacer(Modifier.height(10.dp))

                PoweredByPocketHeader(
                    onLearnMoreClicked = {},
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
                        timesShown = index.toLong(),
                    ),
                )
                false -> add(
                    PocketSponsoredStory(
                        id = index,
                        title = "This is a ${"very ".repeat(index)} long title",
                        url = "https://sponsored-story$index.com",
                        imageUrl = "",
                        sponsor = "Mozilla",
                        shim = PocketSponsoredStoryShim("", ""),
                        priority = index,
                        caps = PocketSponsoredStoryCaps(
                            flightCount = index,
                            flightPeriod = index * 2,
                            lifetimeCount = index * 3,
                        ),
                    ),
                )
            }
        }
    }
}
