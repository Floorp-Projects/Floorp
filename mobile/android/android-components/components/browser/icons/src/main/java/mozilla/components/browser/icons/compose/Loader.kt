/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.compose

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest

/**
 * Loads an icon for the given [url] (or generates one) and makes it available to the inner
 * [IconLoaderScope].
 *
 * The loaded image will be available through the [WithIcon] composable. While the icon is still
 * loading [Placeholder] will get rendered.
 *
 * @param url The URL of the website an icon should be loaded for. Note that this is the URL of the
 * website the icon is *for* (e.g. https://github.com) and not the URL of the icon itself (e.g.
 * https://github.com/favicon.ico)
 * @param size The preferred size of the icon that should be loaded.
 * @param isPrivate Whether or not a private request (like in private browsing) should be used to
 * download the icon (if needed).
 */
@Composable
fun BrowserIcons.Loader(
    url: String,
    size: IconRequest.Size = IconRequest.Size.DEFAULT,
    isPrivate: Boolean = false,
    content: @Composable IconLoaderScope.() -> Unit,
) {
    val request = IconRequest(url, size, emptyList(), null, isPrivate)
    val scope = remember(request) { InternalIconLoaderScope() }

    LaunchedEffect(request) {
        val icon = loadIcon(request).await()
        scope.state.value = IconLoaderState.Icon(
            BitmapPainter(icon.bitmap.asImageBitmap()),
            icon.color,
            icon.source,
            icon.maskable,
        )
    }

    scope.content()
}
