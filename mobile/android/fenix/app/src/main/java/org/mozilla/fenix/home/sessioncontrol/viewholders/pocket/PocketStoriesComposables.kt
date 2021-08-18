/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:OptIn(ExperimentalMaterialApi::class, ExperimentalAnimationApi::class)
@file:Suppress("MagicNumber")

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.ExperimentalAnimationApi
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
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
import androidx.compose.material.Card
import androidx.compose.material.ContentAlpha
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.LocalContentAlpha
import androidx.compose.material.MaterialTheme
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
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.R
import kotlin.random.Random

/**
 * Displays a single [PocketRecommendedStory].
 */
@Composable
fun PocketStory(
    @PreviewParameter(PocketStoryProvider::class) story: PocketRecommendedStory,
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
            modifier = Modifier
                .size(160.dp, 87.dp)
                .padding(bottom = 8.dp)
        ) {
            // Don't yet have a easy way to load URLs in Images.
            // Default to a solid color to make it easy to appreciate dimensions
            Box(Modifier.background(Color.Blue))
            // Image(
            //     painterResource(R.drawable.ic_pdd),
            //     contentDescription = "hero image",
            //     contentScale = ContentScale.FillHeight,
            // )
        }

        CompositionLocalProvider(LocalContentAlpha provides ContentAlpha.medium) {
            Text(
                modifier = Modifier.padding(bottom = 2.dp),
                text = story.domain,
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
    @PreviewParameter(PocketStoryProvider::class) stories: List<PocketRecommendedStory>
) {
    // Items will be shown on two rows. Ceil the divide result to show more items on the top row.
    val halfStoriesIndex = (stories.size + 1) / 2

    LazyRow {
        itemsIndexed(stories) { index, item ->
            if (index < halfStoriesIndex) {
                Column(
                    Modifier.padding(end = if (index == halfStoriesIndex) 0.dp else 8.dp)
                ) {
                    PocketStory(item)

                    Spacer(modifier = Modifier.height(24.dp))

                    stories.getOrNull(halfStoriesIndex + index)?.let {
                        PocketStory(it)
                    }
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

        // Don't yet have the bottom image from designs as a proper Android SVG.
        Box(
            Modifier
                .background(Color.Red)
                .size(64.dp, 27.dp)
                .padding(top = 16.dp)
        )
        // Image(
        //     painterResource(R.drawable.ic_firefox_pocket),
        //     "Firefox and Pocket logos",
        //     Modifier
        //         .size(64.dp, 27.dp)
        //         .padding(top = 16.dp),
        //     contentScale = ContentScale.FillHeight
        // )

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
fun ExpandableCard(content: @Composable (() -> Unit)) {
    var isExpanded by remember { mutableStateOf(true) }
    val chevronRotationState by animateFloatAsState(targetValue = if (isExpanded) 0f else 180f)

    Card(
        modifier = Modifier.fillMaxWidth(),
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
            PocketStories(stories = getFakePocketStories(7))
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
                    id = randomNumber.toLong(),
                    url = "https://story$randomNumber.com",
                    title = "This is a ${"very ".repeat(randomNumber)} long title",
                    domain = "Website no #$randomNumber",
                    excerpt = "FOO",
                    dedupeUrl = "BAR",
                    imageSrc = "",
                    sortId = randomNumber,
                    publishedTimestamp = randomNumber.toString()
                )
            )
        }
    }
}
