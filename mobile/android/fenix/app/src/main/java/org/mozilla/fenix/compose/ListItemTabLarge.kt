/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

const val ITEM_WIDTH = 328
const val ITEM_HEIGHT = 116

/**
 * Default layout of a large tab shown in a list taking String arguments for title and caption.
 * Has the following structure:
 * ```
 * ---------------------------------------------
 * | -------------- Title                      |
 * | |    Image   | wrapped on                 |
 * | |    from    | three rows if needed       |
 * | |  imageUrl  |                            |
 * | -------------- Optional caption           |
 * ---------------------------------------------
 * ```
 *
 * @param imageUrl URL from where the to download a header image of the tab this composable renders.
 * @param title Title off the tab this composable renders.
 * @param caption Optional caption text.
 * @param backgroundColor Background [Color] of the item.
 * @param onClick Optional callback to be invoked when this composable is clicked.
 */
@Composable
fun ListItemTabLarge(
    imageUrl: String,
    title: String,
    caption: String? = null,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    onClick: (() -> Unit)? = null,
) {
    ListItemTabSurface(
        imageUrl = imageUrl,
        backgroundColor = backgroundColor,
        onClick = onClick,
    ) {
        Text(
            text = title,
            color = FirefoxTheme.colors.textPrimary,
            fontSize = 14.sp,
            overflow = TextOverflow.Ellipsis,
            maxLines = 3,
        )

        if (caption != null) {
            Text(
                text = caption,
                color = FirefoxTheme.colors.textSecondary,
                fontSize = 12.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )
        }
    }
}

/**
 * Default layout of a large tab shown in a list taking composable arguments for title and caption
 * allowing as an exception to customize these elements.
 * Has the following structure:
 * ```
 * ---------------------------------------------
 * | -------------- -------------------------- |
 * | |            | |         Title          | |
 * | |    Image   | |       composable       | |
 * | |    from    | -------------------------- |
 * | |  imageUrl  | -------------------------- |
 * | |            | |   Optional composable  | |
 * | -------------- -------------------------- |
 * ---------------------------------------------
 * ```
 *
 * @param imageUrl URL from where the to download a header image of the tab this composable renders.
 * @param backgroundColor Background [Color] of the item.
 * @param onClick Optional callback to be invoked when this composable is clicked.
 * @param title Composable rendering the title of the tab this composable represents.
 * @param subtitle Optional tab caption composable.
 */
@Composable
fun ListItemTabLarge(
    imageUrl: String,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    onClick: () -> Unit,
    title: @Composable () -> Unit,
    subtitle: @Composable (() -> Unit)? = null,
) {
    ListItemTabSurface(
        imageUrl = imageUrl,
        backgroundColor = backgroundColor,
        onClick = onClick,
    ) {
        title()

        subtitle?.invoke()
    }
}

/**
 * Shared default configuration of a ListItemTabLarge Composable.
 *
 * @param imageUrl URL from where the to download a header image of the tab this composable renders.
 * @param backgroundColor Background [Color] of the item.
 * @param contentPadding Padding used for the image and details of the item.
 * @param onClick Optional callback to be invoked when this composable is clicked.
 * @param tabDetails [Composable] Displayed to the the end of the image. Allows for variation in the item text style.
 */
@Composable
fun ListItemTabSurface(
    imageUrl: String,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    contentPadding: PaddingValues = PaddingValues(16.dp),
    onClick: (() -> Unit)? = null,
    tabDetails: @Composable () -> Unit,
) {
    var modifier = Modifier.size(ITEM_WIDTH.dp, ITEM_HEIGHT.dp)
    if (onClick != null) modifier = modifier.then(Modifier.clickable { onClick() })

    Card(
        modifier = modifier,
        shape = RoundedCornerShape(8.dp),
        backgroundColor = backgroundColor,
        elevation = 6.dp,
    ) {
        Row(
            modifier = Modifier.padding(contentPadding),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            val (imageWidth, imageHeight) = 116.dp to 84.dp
            val imageModifier = Modifier
                .size(imageWidth, imageHeight)
                .clip(RoundedCornerShape(8.dp))

            Image(imageUrl, imageModifier, false, imageWidth)

            Spacer(Modifier.width(16.dp))

            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.SpaceBetween,
            ) {
                tabDetails()
            }
        }
    }
}

@Composable
@LightDarkPreview
private fun ListItemTabLargePreview() {
    FirefoxTheme {
        ListItemTabLarge(
            imageUrl = "",
            title = "This is a very long title for a tab but needs to be so for this preview",
            caption = "And this is a caption",
        ) { }
    }
}

@Composable
@LightDarkPreview
private fun ListItemTabSurfacePreview() {
    FirefoxTheme {
        ListItemTabSurface(
            imageUrl = "",
        ) {
            Text(
                text = "This can be anything",
                color = FirefoxTheme.colors.textPrimary,
                fontSize = 22.sp,
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun ListItemTabSurfaceWithCustomBackgroundPreview() {
    FirefoxTheme {
        ListItemTabSurface(
            imageUrl = "",
            backgroundColor = Color.Cyan,
        ) {
            Text(
                text = "This can be anything",
                color = FirefoxTheme.colors.textPrimary,
                fontSize = 22.sp,
            )
        }
    }
}
