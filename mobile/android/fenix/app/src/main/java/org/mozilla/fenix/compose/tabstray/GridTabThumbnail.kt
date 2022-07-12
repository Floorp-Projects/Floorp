/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.tabstray

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import mozilla.components.concept.base.images.ImageLoadRequest
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Card which will display the thumbnail for a tab. If a thumbnail is not available for the [tabId],
 * the favicon [R.drawable.mozac_ic_globe] icon will be displayed.
 *
 * @param tabId Key used to remember the thumbnail for future compositions.
 * @param modifier [Modifier] used to draw the image content.
 * @param contentDescription Text used by accessibility services
 * to describe what this image represents.
 * @param contentScale [ContentScale] used to draw image content.
 * @param alignment [Alignment] used to draw the image content.
 */
@Composable
@Suppress("LongParameterList")
fun GridTabThumbnail(
    tabId: String,
    size: Dp,
    modifier: Modifier = Modifier,
    contentDescription: String? = null,
    contentScale: ContentScale = ContentScale.FillWidth,
    alignment: Alignment = Alignment.TopCenter
) {
    Card(
        modifier = modifier,
        backgroundColor = FirefoxTheme.colors.layer2
    ) {
        if (inComposePreview) {
            GlobeIcon()
        } else {
            val rememberBitmap = remember(tabId) { mutableStateOf<ImageBitmap?>(null) }
            val imageSize = LocalDensity.current.run { size.toPx().toInt() }
            val request = ImageLoadRequest(tabId, imageSize)
            val storage = LocalContext.current.components.core.thumbnailStorage
            val bitmap = rememberBitmap.value

            LaunchedEffect(tabId) {
                rememberBitmap.value = storage.loadThumbnail(request).await()?.asImageBitmap()
            }

            if (bitmap != null) {
                val painter = BitmapPainter(bitmap)
                Image(
                    painter = painter,
                    contentDescription = contentDescription,
                    modifier = modifier,
                    contentScale = contentScale,
                    alignment = alignment
                )
            } else {
                GlobeIcon()
            }
        }
    }
}

/**
 * Globe icon to be displayed when no thumbnail is available.
 */
@Composable
private fun GlobeIcon() {
    Icon(
        painter = painterResource(id = R.drawable.mozac_ic_globe),
        contentDescription = null,
        modifier = Modifier
            .padding(22.dp)
            .fillMaxSize(),
        tint = FirefoxTheme.colors.iconSecondary
    )
}

@Preview
@Composable
private fun ThumbnailCardPreview() {
    FirefoxTheme(theme = Theme.getTheme()) {
        Surface(
            modifier = Modifier
                .fillMaxWidth()
                .height(300.dp)
        ) {
            GridTabThumbnail(
                tabId = "1",
                size = LocalConfiguration.current.screenWidthDp.dp
            )
        }
    }
}
