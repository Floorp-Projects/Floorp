/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import mozilla.components.concept.base.images.ImageLoadRequest
import org.mozilla.fenix.components.components
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Thumbnail belonging to a [ImageLoadRequest]. Asynchronously fetches the bitmap from storage.
 *
 * @param request [ImageLoadRequest] used to fetch the thumbnail bitmap.
 * @param contentScale [ContentScale] used to draw image content.
 * @param alignment [Alignment] used to draw the image content.
 * @param modifier [Modifier] used to draw the image content.
 * @param fallbackContent The content to display with a thumbnail is unable to be loaded.
 */
@Composable
fun ThumbnailImage(
    request: ImageLoadRequest,
    contentScale: ContentScale,
    alignment: Alignment,
    modifier: Modifier = Modifier,
    fallbackContent: @Composable () -> Unit,
) {
    if (inComposePreview) {
        Box(modifier = modifier.background(color = FirefoxTheme.colors.layer3))
    } else {
        var state by remember { mutableStateOf(ThumbnailImageState(null, false)) }
        val scope = rememberCoroutineScope()
        val storage = components.core.thumbnailStorage

        DisposableEffect(Unit) {
            if (!state.hasLoaded) {
                scope.launch {
                    val thumbnailBitmap = storage.loadThumbnail(request).await()
                    thumbnailBitmap?.prepareToDraw()
                    state = ThumbnailImageState(
                        bitmap = thumbnailBitmap,
                        hasLoaded = true,
                    )
                }
            }

            onDispose {
                // Recycle the bitmap to liberate the RAM. Without this, a list of [ThumbnailImage]
                // will bloat the memory. This is a trade-off, however, as the bitmap
                // will be re-fetched if this Composable is disposed and re-loaded.
                state.bitmap?.recycle()
                state = ThumbnailImageState(
                    bitmap = null,
                    hasLoaded = false,
                )
            }
        }

        if (state.bitmap == null && state.hasLoaded) {
            fallbackContent()
        } else {
            state.bitmap?.let { bitmap ->
                Image(
                    painter = BitmapPainter(bitmap.asImageBitmap()),
                    contentDescription = null,
                    modifier = modifier,
                    contentScale = contentScale,
                    alignment = alignment,
                )
            }
        }
    }
}

/**
 * State wrapper for [ThumbnailImage].
 */
private data class ThumbnailImageState(
    val bitmap: Bitmap?,
    val hasLoaded: Boolean,
)

/**
 * This preview does not demo anything. This is to ensure that [ThumbnailImage] does not break other previews.
 */
@Preview
@Composable
private fun ThumbnailImagePreview() {
    FirefoxTheme {
        ThumbnailImage(
            request = ImageLoadRequest("1", 1, false),
            modifier = Modifier.size(50.dp),
            contentScale = ContentScale.Crop,
            alignment = Alignment.Center,
            fallbackContent = {},
        )
    }
}
