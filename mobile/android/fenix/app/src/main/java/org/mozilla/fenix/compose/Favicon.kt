/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import android.content.res.Configuration
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.compose.Placeholder
import mozilla.components.browser.icons.compose.WithIcon
import org.mozilla.fenix.components.components
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Load and display the favicon of a particular website.
 *
 * @param url Website [url] for which the favicon will be shown.
 * @param size [Dp] height and width of the image to be loaded.
 * @param modifier [Modifier] to be applied to the layout.
 * @param isPrivate Whether or not a private request (like in private browsing) should be used to
 * download the icon (if needed).
 */
@Composable
fun Favicon(
    url: String,
    size: Dp,
    modifier: Modifier = Modifier,
    isPrivate: Boolean = false,
) {
    if (inComposePreview) {
        FaviconPlaceholder(
            size = size,
            modifier = modifier,
        )
    } else {
        components.core.icons.LoadableImage(
            url = url,
            isPrivate = isPrivate,
            iconSize = size.toIconRequestSize(),
        ) {
            Placeholder {
                FaviconPlaceholder(
                    size = size,
                    modifier = modifier,
                )
            }

            WithIcon { icon ->
                Image(
                    painter = icon.painter,
                    contentDescription = null,
                    modifier = modifier
                        .size(size)
                        .clip(RoundedCornerShape(2.dp)),
                    contentScale = ContentScale.Crop,
                )
            }
        }
    }
}

/**
 * Placeholder used while the Favicon image is loading.
 *
 * @param size [Dp] height and width of the image.
 * @param modifier [Modifier] allowing to control among others the dimensions and shape of the image.
 */
@Composable
private fun FaviconPlaceholder(
    size: Dp,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier
            .size(size)
            .clip(RoundedCornerShape(2.dp))
            .background(
                color = FirefoxTheme.colors.layer2,
            ),
    )
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun FaviconPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            Favicon(
                url = "www.mozilla.com",
                size = 64.dp,
            )
        }
    }
}

@Composable
private fun Dp.toIconRequestSize() = when {
    value <= dimensionResource(IconRequest.Size.DEFAULT.dimen).value -> IconRequest.Size.DEFAULT
    value <= dimensionResource(IconRequest.Size.LAUNCHER.dimen).value -> IconRequest.Size.LAUNCHER
    else -> IconRequest.Size.LAUNCHER_ADAPTIVE
}
