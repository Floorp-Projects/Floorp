/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.browser.icons.compose.Loader
import mozilla.components.browser.icons.compose.Placeholder
import mozilla.components.browser.icons.compose.WithIcon
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.concept.base.images.ImageLoadRequest
import org.mozilla.fenix.components.components
import org.mozilla.fenix.theme.FirefoxTheme

private const val THUMBNAIL_SIZE = 108
private const val FALLBACK_ICON_SIZE = 36

/**
 * Card which will display a thumbnail. If a thumbnail is not available for [url], the favicon
 * will be displayed until the thumbnail is loaded.
 *
 * @param url Url to display thumbnail for.
 * @param request [ImageLoadRequest] used to fetch the thumbnail bitmap.
 * @param storage [ThumbnailStorage] to obtain tab thumbnail bitmaps from.
 * @param modifier [Modifier] used to draw the image content.
 * @param backgroundColor [Color] used for the background of the favicon.
 * @param contentDescription Text used by accessibility services
 * to describe what this image represents.
 * @param contentScale [ContentScale] used to draw image content.
 * @param alignment [Alignment] used to draw the image content.
 */
@Composable
fun ThumbnailCard(
    url: String,
    request: ImageLoadRequest,
    storage: ThumbnailStorage,
    modifier: Modifier = Modifier,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    contentDescription: String? = null,
    contentScale: ContentScale = ContentScale.FillWidth,
    alignment: Alignment = Alignment.TopCenter,
) {
    Card(
        modifier = modifier,
        backgroundColor = backgroundColor,
    ) {
        ThumbnailImage(
            request = request,
            storage = storage,
            modifier = modifier,
            contentScale = contentScale,
            alignment = alignment,
        ) {
            components.core.icons.Loader(url) {
                Placeholder {
                    Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer3))
                }

                WithIcon { icon ->
                    Box(
                        modifier = Modifier.size(FALLBACK_ICON_SIZE.dp),
                        contentAlignment = Alignment.Center,
                    ) {
                        Image(
                            painter = icon.painter,
                            contentDescription = contentDescription,
                            modifier = Modifier
                                .size(FALLBACK_ICON_SIZE.dp)
                                .clip(RoundedCornerShape(8.dp)),
                            contentScale = contentScale,
                        )
                    }
                }
            }
        }
    }
}

@Preview
@Composable
private fun ThumbnailCardPreview() {
    FirefoxTheme {
        ThumbnailCard(
            url = "https://mozilla.com",
            request = ImageLoadRequest("123", THUMBNAIL_SIZE, false),
            storage = ThumbnailStorage(LocalContext.current),
            modifier = Modifier
                .size(THUMBNAIL_SIZE.dp)
                .clip(RoundedCornerShape(8.dp)),
        )
    }
}
