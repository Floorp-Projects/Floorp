/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.concept.fetch.Client
import org.mozilla.fenix.theme.FirefoxTheme

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
 * @param client [Client] instance to be used for downloading the image.
 * When using [GeckoViewFetchClient] the image will automatically be cached if it has the right headers.
 * @param imageUrl URL from where the to download a header image of the tab this composable renders.
 * @param title Title off the tab this composable renders.
 * @param caption Optional caption text.
 * @param onClick Optional callback to be invoked when this composable is clicked.
 */
@Composable
fun ListItemTabLarge(
    client: Client,
    imageUrl: String,
    title: String,
    caption: String? = null,
    onClick: (() -> Unit)? = null
) {
    ListItemTabSurface(client, imageUrl, onClick) {
        TabTitle(text = title, maxLines = 3)

        if (caption != null) {
            TabSubtitle(text = caption)
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
 * @param client [Client] instance to be used for downloading the image.
 * When using [GeckoViewFetchClient] the image will automatically be cached if it has the right headers.
 * @param imageUrl URL from where the to download a header image of the tab this composable renders.
 * @param title Composable rendering the title of the tab this composable represents.
 * @param subtitle Optional tab caption composable.
 * @param onClick Optional callback to be invoked when this composable is clicked.
 */
@Composable
fun ListItemTabLarge(
    client: Client,
    imageUrl: String,
    onClick: () -> Unit,
    title: @Composable () -> Unit,
    subtitle: @Composable (() -> Unit)? = null
) {
    ListItemTabSurface(client, imageUrl, onClick) {
        title()

        subtitle?.invoke()
    }
}

/**
 * Shared default configuration of a ListItemTabLarge Composable.
 *
 * @param client [Client] instance to be used for downloading the image.
 * When using [GeckoViewFetchClient] the image will automatically be cached if it has the right headers.
 * @param imageUrl URL from where the to download a header image of the tab this composable renders.
 * @param onClick Optional callback to be invoked when this composable is clicked.
 * @param tabDetails [Composable] Displayed to the the end of the image. Allows for variation in the item text style.
 */
@Composable
private fun ListItemTabSurface(
    client: Client,
    imageUrl: String,
    onClick: (() -> Unit)? = null,
    tabDetails: @Composable () -> Unit
) {
    var modifier = Modifier.size(328.dp, 116.dp)
    if (onClick != null) modifier = modifier.then(Modifier.clickable { onClick() })

    Card(
        modifier = modifier,
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.surface,
        elevation = 6.dp
    ) {
        Row(
            modifier = Modifier.padding(16.dp)
        ) {
            val (imageWidth, imageHeight) = 116.dp to 84.dp
            val imageModifier = Modifier
                .size(imageWidth, imageHeight)
                .clip(RoundedCornerShape(8.dp))

            Image(client, imageUrl, imageModifier, false, imageWidth)

            Spacer(Modifier.width(16.dp))

            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.SpaceBetween
            ) {
                tabDetails()
            }
        }
    }
}

@Composable
@Preview
private fun ListItemTabLargePreview() {
    FirefoxTheme {
        ListItemTabLarge(
            client = FakeClient(),
            imageUrl = "",
            title = "This is a very long title for a tab but needs to be so for this preview",
            caption = "And this is a caption"
        ) { }
    }
}
