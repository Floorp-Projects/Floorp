/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import mozilla.components.support.images.compose.loader.ImageLoader
import mozilla.components.support.images.compose.loader.WithImage
import org.mozilla.fenix.components.components
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A composable that lays out and draws the image from a given URL while showing a default placeholder
 * while that image is downloaded or a default fallback image when downloading failed.
 *
 * @param url URL from where the to download the image to be shown.
 * @param modifier [Modifier] to be applied to the layout.
 * @param private Whether or not this is a private request. Like in private browsing mode,
 * private requests will not cache anything on disk and not send any cookies shared with the browser.
 * @param targetSize Image size (width and height) the loaded image should be scaled to.
 * @param contentDescription Localized text used by accessibility services to describe what this image represents.
 * This should always be provided unless this image is used for decorative purposes, and does not represent
 * a meaningful action that a user can take.
 * @param alignment Optional alignment parameter used to place the [Painter] in the given
 * bounds defined by the width and height.
 * @param contentScale Optional scale parameter used to determine the aspect ratio scaling to be used
 * if the bounds are a different size from the intrinsic size of the [Painter].
 * @param placeholder composable displayed while the image is still loading.
 * By default set to a solid color in [DefaultImagePlaceholder].
 * @param fallback composable displayed when the image fails loading.
 * By default set to a solid color in [DefaultImagePlaceholder].
 */
@Composable
fun Image(
    url: String,
    modifier: Modifier = Modifier,
    private: Boolean = false,
    targetSize: Dp = 100.dp,
    contentDescription: String? = null,
    alignment: Alignment = Alignment.Center,
    contentScale: ContentScale = ContentScale.Fit,
    placeholder: @Composable () -> Unit = { DefaultImagePlaceholder(modifier, contentDescription) },
    fallback: @Composable () -> Unit = { DefaultImagePlaceholder(modifier, contentDescription) },
) {
    if (inComposePreview) {
        placeholder()
    } else {
        ImageLoader(
            url = url,
            client = components.core.client,
            private = private,
            targetSize = targetSize,
        ) {
            WithImage { painter ->
                androidx.compose.foundation.Image(
                    painter = painter,
                    modifier = modifier,
                    contentDescription = contentDescription,
                    alignment = alignment,
                    contentScale = contentScale,
                )
            }

            WithPlaceholder(placeholder)

            WithFallback(fallback)
        }
    }
}

@Composable
@Preview
private fun ImagePreview() {
    FirefoxTheme {
        Image(
            url = "https://mozilla.com",
            modifier = Modifier.height(100.dp).width(200.dp),
        )
    }
}
