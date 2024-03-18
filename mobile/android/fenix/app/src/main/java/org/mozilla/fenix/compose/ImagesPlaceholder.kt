/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.painter.ColorPainter
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.support.images.compose.loader.Fallback
import mozilla.components.support.images.compose.loader.ImageLoaderScope
import mozilla.components.support.images.compose.loader.Placeholder
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Renders the app image placeholder while the image is still getting loaded.
 *
 * @param placeholder [Composable] composable used during loading.
 * By default, set to [DefaultImagePlaceholder] in [org.mozilla.fenix.compose.Image].
 */
@Composable
internal fun ImageLoaderScope.WithPlaceholder(
    placeholder: @Composable () -> Unit,
) {
    Placeholder {
        placeholder()
    }
}

/**
 * Renders the app image placeholder if loading image failed.
 *
 * @param fallback [Painter] composable used if loading failed.
 * By default, set to [DefaultImagePlaceholder] in [org.mozilla.fenix.compose.Image].
 */
@Composable
internal fun ImageLoaderScope.WithFallback(
    fallback: @Composable () -> Unit,
) {
    Fallback {
        fallback()
    }
}

/**
 * Application default image placeholder.
 *
 * @param modifier [Modifier] allowing to control among others the dimensions and shape of the image.
 * @param contentDescription Text provided to accessibility services to describe what this image represents.
 * Defaults to [null] suited for an image used only for decorative purposes and not to be read by
 * accessibility services.
 */
@Composable
internal fun DefaultImagePlaceholder(
    modifier: Modifier,
    contentDescription: String? = null,
) {
    Image(ColorPainter(FirefoxTheme.colors.layer2), contentDescription, modifier)
}

@Composable
@Preview
private fun DefaultImagePlaceholderPreview() {
    FirefoxTheme {
        DefaultImagePlaceholder(
            modifier = Modifier
                .size(200.dp, 100.dp)
                .clip(RoundedCornerShape(8.dp)),
        )
    }
}
