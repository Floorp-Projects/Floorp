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
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.browser.icons.compose.Loader
import mozilla.components.browser.icons.compose.Placeholder
import mozilla.components.browser.icons.compose.WithIcon
import mozilla.components.concept.base.images.ImageLoadRequest
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Card which will display a thumbnail. If a thumbnail is not available for [url], the favicon
 * will be displayed until the thumbnail is loaded.
 *
 * @param url Url to display thumbnail for.
 * @param key Key used to remember the thumbnail for future compositions.
 * @param modifier [Modifier] used to draw the image content.
 * @param contentDescription Text used by accessibility services
 * to describe what this image represents.
 * @param contentScale [ContentScale] used to draw image content.
 * @param alignment [Alignment] used to draw the image content.
 */
@Composable
fun ThumbnailCard(
    url: String,
    key: String,
    modifier: Modifier = Modifier,
    contentDescription: String? = null,
    contentScale: ContentScale = ContentScale.FillWidth,
    alignment: Alignment = Alignment.TopCenter
) {
    Card(
        modifier = modifier,
        backgroundColor = colorResource(id = R.color.photonGrey20)
    ) {
        if (inComposePreview) {
            Box(
                modifier = Modifier.background(color = FirefoxTheme.colors.layer3)
            )
        } else {
            components.core.icons.Loader(url) {
                Placeholder {
                    Box(
                        modifier = Modifier.background(color = FirefoxTheme.colors.layer3)
                    )
                }

                WithIcon { icon ->
                    Box(
                        modifier = Modifier.size(36.dp),
                        contentAlignment = Alignment.Center
                    ) {
                        Image(
                            painter = icon.painter,
                            contentDescription = contentDescription,
                            modifier = Modifier
                                .size(36.dp)
                                .clip(RoundedCornerShape(8.dp)),
                            contentScale = ContentScale.Fit
                        )
                    }
                }
            }

            ThumbnailImage(
                key = key,
                modifier = modifier,
                contentScale = contentScale,
                alignment = alignment
            )
        }
    }
}

@Composable
private fun ThumbnailImage(
    key: String,
    modifier: Modifier,
    contentScale: ContentScale,
    alignment: Alignment
) {
    val rememberBitmap = remember(key) { mutableStateOf<ImageBitmap?>(null) }
    val size = LocalDensity.current.run { 108.dp.toPx().toInt() }
    val request = ImageLoadRequest(key, size)
    val storage = components.core.thumbnailStorage
    val bitmap = rememberBitmap.value

    LaunchedEffect(key) {
        rememberBitmap.value = storage.loadThumbnail(request).await()?.asImageBitmap()
    }

    if (bitmap != null) {
        val painter = BitmapPainter(bitmap)
        Image(
            painter = painter,
            contentDescription = null,
            modifier = modifier,
            contentScale = contentScale,
            alignment = alignment
        )
    }
}

@Preview
@Composable
private fun ThumbnailCardPreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        ThumbnailCard(
            url = "https://mozilla.com",
            key = "123",
            modifier = Modifier
                .size(108.dp, 80.dp)
                .clip(RoundedCornerShape(8.dp))
        )
    }
}
