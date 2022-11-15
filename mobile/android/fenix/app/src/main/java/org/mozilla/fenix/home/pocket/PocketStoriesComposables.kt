/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber")

package org.mozilla.fenix.home.pocket

import android.graphics.Rect
import android.net.Uri
import androidx.annotation.FloatRange
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
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toAndroidRect
import androidx.compose.ui.layout.LayoutCoordinates
import androidx.compose.ui.layout.boundsInWindow
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryCaps
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryShim
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ClickableSubstringLink
import org.mozilla.fenix.compose.EagerFlingBehavior
import org.mozilla.fenix.compose.ListItemTabLarge
import org.mozilla.fenix.compose.ListItemTabLargePlaceholder
import org.mozilla.fenix.compose.ListItemTabSurface
import org.mozilla.fenix.compose.SelectableChip
import org.mozilla.fenix.compose.StaggeredHorizontalGrid
import org.mozilla.fenix.compose.TabSubtitleWithInterdot
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

private const val URI_PARAM_UTM_KEY = "utm_source"
private const val POCKET_STORIES_UTM_VALUE = "pocket-newtab-android"
private const val POCKET_FEATURE_UTM_KEY_VALUE = "utm_source=ff_android"

/**
 * The Pocket section may appear first on the homescreen and be fully constructed
 * to then be pushed downwards when other elements appear.
 * This can lead to overcounting impressions with multiple such events being possible
 * without the user actually having time to see the stories or scrolling to see the Pocket section.
 */
private const val MINIMUM_TIME_TO_SETTLE_MS = 1000

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
        backgroundColor = backgroundColor,
        onClick = { onStoryClick(story) },
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

        Spacer(Modifier.height(9.dp))

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

        Spacer(Modifier.height(7.dp))

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
@Suppress("LongParameterList")
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

    LazyRow(
        modifier = Modifier.semantics {
            testTagsAsResourceId = true
            testTag = "pocket.stories"
        },
        contentPadding = PaddingValues(horizontal = contentPadding),
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
                            ListItemTabLargePlaceholder(stringResource(R.string.pocket_stories_placeholder_text)) {
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
                            Box(
                                modifier = Modifier.onShown(0.5f) {
                                    onStoryShown(story, rowIndex to columnIndex)
                                },
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

/**
 * Add a callback for when this Composable is "shown" on the screen.
 * This checks whether the composable has at least [threshold] ratio of it's total area drawn inside
 * the screen bounds.
 * Does not account for other Views / Windows covering it.
 */
private fun Modifier.onShown(
    @FloatRange(from = 0.0, to = 1.0) threshold: Float,
    onVisible: () -> Unit,
): Modifier {
    val initialTime = System.currentTimeMillis()
    var lastVisibleCoordinates: LayoutCoordinates? = null

    return composed {
        if (inComposePreview) {
            Modifier
        } else {
            val context = LocalContext.current
            var wasEventReported by remember { mutableStateOf(false) }

            val toolbarHeight = context.resources.getDimensionPixelSize(R.dimen.browser_toolbar_height)
            val isToolbarPlacedAtBottom = context.settings().shouldUseBottomToolbar
            // Get a Rect of the entire screen minus system insets minus the toolbar
            val screenBounds = Rect()
                .apply { LocalView.current.getWindowVisibleDisplayFrame(this) }
                .apply {
                    when (isToolbarPlacedAtBottom) {
                        true -> bottom -= toolbarHeight
                        false -> top += toolbarHeight
                    }
                }

            // In the event this composable starts as visible but then gets pushed offscreen
            // before MINIMUM_TIME_TO_SETTLE_MS we will not report is as being visible.
            // In the LaunchedEffect we add support for when the composable starts as visible and then
            // it's position isn't changed after MINIMUM_TIME_TO_SETTLE_MS so it must be reported as visible.
            LaunchedEffect(initialTime) {
                delay(MINIMUM_TIME_TO_SETTLE_MS.toLong())
                if (!wasEventReported && lastVisibleCoordinates?.isVisible(screenBounds, threshold) == true) {
                    wasEventReported = true
                    onVisible()
                }
            }

            onGloballyPositioned { coordinates ->
                if (!wasEventReported && coordinates.isVisible(screenBounds, threshold)) {
                    if (System.currentTimeMillis() - initialTime > MINIMUM_TIME_TO_SETTLE_MS) {
                        wasEventReported = true
                        onVisible()
                    } else {
                        lastVisibleCoordinates = coordinates
                    }
                }
            }
        }
    }
}

/**
 * Return whether this has at least [threshold] ratio of it's total area drawn inside
 * the screen bounds.
 */
private fun LayoutCoordinates.isVisible(
    visibleRect: Rect,
    @FloatRange(from = 0.0, to = 1.0) threshold: Float,
): Boolean {
    if (!isAttached) return false

    return boundsInWindow().toAndroidRect().getIntersectPercentage(size, visibleRect) >= threshold
}

/**
 * Returns the ratio of how much this intersects with [other].
 *
 * @param realSize [IntSize] containing the true height and width of the composable.
 * @param other Other [Rect] for whcih to check the intersection area.
 *
 * @return A `0..1` float range for how much this [Rect] intersects with other.
 */
@FloatRange(from = 0.0, to = 1.0)
private fun Rect.getIntersectPercentage(realSize: IntSize, other: Rect): Float {
    val composableArea = realSize.height * realSize.width
    val heightOverlap = max(0, min(bottom, other.bottom) - max(top, other.top))
    val widthOverlap = max(0, min(right, other.right) - max(left, other.left))
    val intersectionArea = heightOverlap * widthOverlap

    return (intersectionArea.toFloat() / composableArea)
}

/**
 * Displays a list of [PocketRecommendedStoriesCategory]s.
 *
 * @param categories The categories needed to be displayed.
 * @param selections List of categories currently selected.
 * @param modifier [Modifier] to be applied to the layout.
 * @param categoryColors The color set defined by [PocketStoriesCategoryColors] used to style Pocket categories.
 * @param onCategoryClick Callback for when the user taps a category.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Suppress("LongParameterList")
@Composable
fun PocketStoriesCategories(
    categories: List<PocketRecommendedStoriesCategory>,
    selections: List<PocketRecommendedStoriesSelectedCategory>,
    modifier: Modifier = Modifier,
    categoryColors: PocketStoriesCategoryColors = PocketStoriesCategoryColors.buildColors(),
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
                    selectedTextColor = categoryColors.selectedTextColor,
                    unselectedTextColor = categoryColors.unselectedTextColor,
                    selectedBackgroundColor = categoryColors.selectedBackgroundColor,
                    unselectedBackgroundColor = categoryColors.unselectedBackgroundColor,
                ) {
                    onCategoryClick(category)
                }
            }
        }
    }
}

/**
 * Wrapper for the color parameters of [PocketStoriesCategories].
 *
 * @param selectedTextColor Text [Color] when the category is selected.
 * @param unselectedTextColor Text [Color] when the category is not selected.
 * @param selectedBackgroundColor Background [Color] when the category is selected.
 * @param unselectedBackgroundColor Background [Color] when the category is not selected.
 */
data class PocketStoriesCategoryColors(
    val selectedBackgroundColor: Color,
    val unselectedBackgroundColor: Color,
    val selectedTextColor: Color,
    val unselectedTextColor: Color,
) {
    companion object {

        /**
         * Builder function used to construct an instance of [PocketStoriesCategoryColors].
         */
        @Composable
        fun buildColors(
            selectedBackgroundColor: Color = FirefoxTheme.colors.textActionPrimary,
            unselectedBackgroundColor: Color = FirefoxTheme.colors.textActionTertiary,
            selectedTextColor: Color = FirefoxTheme.colors.actionPrimary,
            unselectedTextColor: Color = FirefoxTheme.colors.actionTertiary,
        ) = PocketStoriesCategoryColors(
            selectedBackgroundColor = selectedBackgroundColor,
            unselectedBackgroundColor = unselectedBackgroundColor,
            selectedTextColor = selectedTextColor,
            unselectedTextColor = unselectedTextColor,
        )
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

            Column {
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
    FirefoxTheme(theme = Theme.getTheme()) {
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
