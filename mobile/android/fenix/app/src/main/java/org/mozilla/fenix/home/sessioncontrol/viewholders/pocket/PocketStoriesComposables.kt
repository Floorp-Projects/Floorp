/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:OptIn(ExperimentalMaterialApi::class, ExperimentalAnimationApi::class)
@file:Suppress("MagicNumber")

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.ExperimentalAnimationApi
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.Image
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.ClickableText
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Card
import androidx.compose.material.ContentAlpha
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.LocalContentAlpha
import androidx.compose.material.MaterialTheme
import androidx.compose.material.OutlinedButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.layout.Layout
import androidx.compose.ui.layout.Placeable
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.support.images.compose.loader.Fallback
import mozilla.components.support.images.compose.loader.ImageLoader
import mozilla.components.support.images.compose.loader.Placeholder
import mozilla.components.support.images.compose.loader.WithImage
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import kotlin.math.roundToInt
import kotlin.random.Random

/**
 * Displays a single [PocketRecommendedStory].
 */
@Composable
fun PocketStory(
    @PreviewParameter(PocketStoryProvider::class) story: PocketRecommendedStory,
    client: Client,
    modifier: Modifier = Modifier
) {
    Column(
        modifier
            .size(160.dp, 191.dp)
            .clip(RoundedCornerShape(4.dp))
            .clickable { /* no-op */ }
    ) {
        Card(
            elevation = 6.dp,
            shape = RoundedCornerShape(4.dp),
            modifier = Modifier.size(160.dp, 87.dp)
        ) {
            ImageLoader(
                client = client,
                // The endpoint allows us to ask for the optimal resolution image.
                url = story.imageUrl.replace(
                    "{wh}",
                    with(LocalDensity.current) {
                        "${160.dp.toPx().roundToInt()}x${87.dp.toPx().roundToInt()}"
                    }
                ),
                targetSize = 160.dp
            ) {
                WithImage { painter ->
                    Image(
                        painter,
                        modifier = Modifier.size(160.dp, 87.dp),
                        contentDescription = "${story.title} story image"
                    )
                }

                Placeholder {
                    Box(
                        Modifier.background(
                            when (isSystemInDarkTheme()) {
                                true -> Color(0xFF42414D) // DarkGrey30
                                false -> PhotonColors.LightGrey30
                            }
                        )
                    )
                }

                Fallback {
                    Box(
                        Modifier.background(
                            when (isSystemInDarkTheme()) {
                                true -> Color(0xFF42414D) // DarkGrey30
                                false -> PhotonColors.LightGrey30
                            }
                        )
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(8.dp))

        CompositionLocalProvider(LocalContentAlpha provides ContentAlpha.medium) {
            Text(
                modifier = Modifier.padding(bottom = 2.dp),
                text = story.publisher,
                style = MaterialTheme.typography.caption,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
        Text(
            text = story.title,
            style = MaterialTheme.typography.subtitle1,
            maxLines = 4,
            overflow = TextOverflow.Ellipsis
        )
    }
}

/**
 * Displays a list of [PocketRecommendedStory]es.
 */
@Composable
fun PocketStories(
    @PreviewParameter(PocketStoryProvider::class) stories: List<PocketRecommendedStory>,
    client: Client
) {
    // Items will be shown on two rows. Ceil the divide result to show more items on the top row.
    val halfStoriesIndex = (stories.size + 1) / 2

    LazyRow {
        itemsIndexed(stories) { index, item ->
            if (index < halfStoriesIndex) {
                Column(
                    Modifier.padding(end = if (index == halfStoriesIndex) 0.dp else 8.dp)
                ) {
                    PocketStory(item, client)

                    Spacer(modifier = Modifier.height(24.dp))

                    stories.getOrNull(halfStoriesIndex + index)?.let {
                        PocketStory(it, client)
                    }
                }
            }
        }
    }
}

/**
 * Displays a list of [PocketRecommendedStoryCategory].
 *
 * @param categories the categories needed to be displayed.
 * @param onCategoryClick callback for when the user taps a category.
 */
@Composable
fun PocketStoriesCategories(
    categories: List<PocketRecommendedStoryCategory>,
    onCategoryClick: (PocketRecommendedStoryCategory) -> Unit
) {
    StaggeredHorizontalGrid {
        categories.forEach { category ->
            PocketStoryCategory(category) {
                onCategoryClick(it)
            }
        }
    }
}

/**
 * Displays an individual [PocketRecommendedStoryCategory].
 *
 * @param category the categories needed to be displayed.
 * @param onClick callback for when the user taps this category.
 */
@Composable
fun PocketStoryCategory(
    category: PocketRecommendedStoryCategory,
    onClick: (PocketRecommendedStoryCategory) -> Unit
) {
    val contentColor = when (category.isSelected) {
        true -> Color.Blue
        false -> Color.DarkGray
    }

    OutlinedButton(
        onClick = { onClick(category) },
        shape = RoundedCornerShape(32.dp),
        border = BorderStroke(1.dp, contentColor),
        colors = ButtonDefaults.outlinedButtonColors(
            contentColor = contentColor
        ),
        contentPadding = PaddingValues(8.dp, 7.dp)
    ) {
        Row {
            Text(
                text = category.name,
                modifier = Modifier.alignByBaseline(),
            )
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_check),
                contentDescription = "Expand or collapse Pocket recommended stories",
                modifier = Modifier.alignByBaseline()
            )
        }
    }
}

/**
 * Displays a list of items as a staggered horizontal grid placing them on ltr rows and continuing
 * on as many below rows as needed to place all items.
 *
 * In an effort to best utilize the available row space this can mix the items such that narrower ones
 * are placed on the same row as wider ones if the otherwise next item doesn't fit.
 *
 * @param modifier to be applied to the layout.
 * @param horizontalItemsSpacing minimum horizontal space between items. Does not add spacing to layout bounds.
 * @param verticalItemsSpacing vertical space between items
 * @param arrangement how the items will be horizontally aligned and spaced.
 * @param content the children composables to be laid out.
 */
@Composable
fun StaggeredHorizontalGrid(
    modifier: Modifier = Modifier,
    horizontalItemsSpacing: Dp = 0.dp,
    verticalItemsSpacing: Dp = 8.dp,
    arrangement: Arrangement.Horizontal = Arrangement.SpaceEvenly,
    content: @Composable () -> Unit
) {
    Layout(content, modifier) { items, constraints ->
        val horizontalItemsSpacingPixels = horizontalItemsSpacing.roundToPx()
        val verticalItemsSpacingPixels = verticalItemsSpacing.roundToPx()
        var totalHeight = 0
        val itemsRows = mutableListOf<List<Placeable>>()
        val notYetPlacedItems = items.map {
            it.measure(constraints)
        }.toMutableList()

        fun getIndexOfNextPlaceableThatFitsRow(available: List<Placeable>, currentWidth: Int): Int {
            return available.indexOfFirst {
                currentWidth + it.width <= constraints.maxWidth
            }
        }

        // Populate each row with as many items as possible combining wider with narrower items.
        // This will change the order of shown categories.
        var (currentRow, currentWidth) = mutableListOf<Placeable>() to 0
        while (notYetPlacedItems.isNotEmpty()) {
            if (currentRow.isEmpty()) {
                currentRow.add(
                    notYetPlacedItems[0].also {
                        currentWidth += it.width
                        totalHeight += it.height + verticalItemsSpacingPixels
                    }
                )
                notYetPlacedItems.removeAt(0)
            } else {
                val nextPlaceableThatFitsIndex = getIndexOfNextPlaceableThatFitsRow(notYetPlacedItems, currentWidth)
                if (nextPlaceableThatFitsIndex >= 0) {
                    currentRow.add(
                        notYetPlacedItems[nextPlaceableThatFitsIndex].also {
                            currentWidth += it.width + horizontalItemsSpacingPixels
                        }
                    )
                    notYetPlacedItems.removeAt(nextPlaceableThatFitsIndex)
                } else {
                    itemsRows.add(currentRow)
                    currentRow = mutableListOf()
                    currentWidth = 0
                }
            }
        }
        if (currentRow.isNotEmpty()) {
            itemsRows.add(currentRow)
        }
        totalHeight -= verticalItemsSpacingPixels

        // Place each item from each row on screen.
        layout(constraints.maxWidth, totalHeight) {
            itemsRows.forEachIndexed { rowIndex, itemRow ->
                val itemsSizes = IntArray(itemRow.size) {
                    itemRow[it].width +
                        if (it < itemRow.lastIndex) horizontalItemsSpacingPixels else 0
                }
                val itemsPositions = IntArray(itemsSizes.size) { 0 }
                with(arrangement) {
                    arrange(constraints.maxWidth, itemsSizes, LayoutDirection.Ltr, itemsPositions)
                }

                itemRow.forEachIndexed { itemIndex, item ->
                    item.place(
                        x = itemsPositions[itemIndex],
                        y = (rowIndex * item.height) + (rowIndex * verticalItemsSpacingPixels)
                    )
                }
            }
        }
    }
}

/**
 * Displays [content] in a layout which will have at the bottom more information about Pocket
 * and also an external link for more up-to-date content.
 */
@Composable
fun PocketRecommendations(
    content: @Composable (() -> Unit)
) {
    val annotatedText = buildAnnotatedString {
        val text = "Pocket is part of the Firefox family. "
        val link = "Learn more."
        val annotationStartIndex = text.length
        val annotationEndIndex = annotationStartIndex + link.length

        append(text + link)

        addStyle(
            SpanStyle(textDecoration = TextDecoration.Underline),
            start = annotationStartIndex,
            end = annotationEndIndex
        )

        addStringAnnotation(
            tag = "link",
            annotation = "https://www.mozilla.org/en-US/firefox/pocket/",
            start = annotationStartIndex,
            end = annotationEndIndex
        )
    }

    Column(
        modifier = Modifier.padding(vertical = 16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        content()

        CompositionLocalProvider(LocalContentAlpha provides ContentAlpha.medium) {
            ClickableText(
                text = annotatedText,
                style = MaterialTheme.typography.caption,
                onClick = {
                    annotatedText
                        .getStringAnnotations("link", it, it)
                        .firstOrNull()?.let {
                            println("Learn more clicked! Should now access ${it.item}")
                        }
                }
            )
        }
    }
}

/**
 * Displays [content] in an expandable card.
 */
@Composable
fun ExpandableCard(
    modifier: Modifier = Modifier,
    content: @Composable (() -> Unit)
) {
    var isExpanded by remember { mutableStateOf(true) }
    val chevronRotationState by animateFloatAsState(targetValue = if (isExpanded) 0f else 180f)

    Card(
        modifier = modifier,
        shape = RoundedCornerShape(4.dp),
        onClick = { isExpanded = !isExpanded }
    ) {
        Column {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    modifier = Modifier.weight(10f),
                    text = "Trending stories from Pocket",
                    style = MaterialTheme.typography.h6,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )

                IconButton(
                    onClick = { isExpanded = !isExpanded },
                    modifier = Modifier.rotate(chevronRotationState)
                ) {
                    Icon(
                        modifier = Modifier.weight(1f),
                        painter = painterResource(id = R.drawable.ic_chevron_up),
                        contentDescription = "Expand or collapse Pocket recommended stories",
                    )
                }
            }

            AnimatedVisibility(visible = isExpanded) {
                content()
            }
        }
    }
}

@Composable
@Preview
private fun FinalDesign() {
    ExpandableCard {
        PocketRecommendations {
            PocketStories(
                stories = getFakePocketStories(7),
                client = FakeClient()
            )

            Spacer(Modifier.height(8.dp))

            PocketStoriesCategories(
                listOf("general", "health", "technology", "food", "career").map {
                    PocketRecommendedStoryCategory(it)
                }
            ) { }
        }
    }
}

private class PocketStoryProvider : PreviewParameterProvider<PocketRecommendedStory> {
    override val values = getFakePocketStories(7).asSequence()
    override val count = 7
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
                    category = "Category #$randomNumber"
                )
            )
        }
    }
}

private class FakeClient : Client() {
    override fun fetch(request: Request) = Response(
        url = request.url,
        status = 200,
        body = Response.Body.empty(),
        headers = MutableHeaders()
    )
}
