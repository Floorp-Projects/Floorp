/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.support.images.compose.loader.ImageLoader
import mozilla.components.support.images.compose.loader.WithImage

/**
 * A composable that lays out and draws the image from a given URL while showing a default placeholder
 * while that image is downloaded or a default fallback image when downloading failed.
 *
 * @param client [Client] instance to be used for downloading the image.
 * When using [GeckoViewFetchClient] the image will automatically be cached if it has the right headers.
 * @param url URL from where the to download the image to be shown.
 * @param modifier [Modifier] to be applied to the layout.
 * @param private Whether or not this is a private request. Like in private browsing mode,
 * private requests will not cache anything on disk and not send any cookies shared with the browser.
 * @param targetSize Image size (width and height) the loaded image should be scaled to.
 * @param contentDescription Localized text used by accessibility services to describe what this image represents.
 * This should always be provided unless this image is used for decorative purposes, and does not represent
 * a meaningful action that a user can take.
 */
@Composable
@Suppress("LongParameterList")
fun Image(
    client: Client,
    url: String,
    modifier: Modifier = Modifier,
    private: Boolean = false,
    targetSize: Dp = 100.dp,
    contentDescription: String? = null
) {
    ImageLoader(
        url = url,
        client = client,
        private = private,
        targetSize = targetSize
    ) {
        WithImage { painter ->
            androidx.compose.foundation.Image(
                painter = painter,
                modifier = modifier,
                contentDescription = contentDescription,
            )
        }

        WithDefaultPlaceholder(modifier, contentDescription)

        WithDefaultFallback(modifier, contentDescription)
    }
}

@Composable
@Preview
private fun ImagePreview() {
    Image(
        FakeClient(),
        "https://mozilla.com",
        Modifier.height(100.dp).width(200.dp)
    )
}

internal class FakeClient : Client() {
    override fun fetch(request: Request) = Response(
        url = request.url,
        status = 200,
        body = Response.Body.empty(),
        headers = MutableHeaders()
    )
}
