/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images.compose.loader

import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.painter.Painter
import mozilla.components.concept.fetch.Client
import mozilla.components.support.images.DesiredSize
import java.util.concurrent.TimeUnit

/**
 * The scope of an [ImageLoader] block.
 *
 * @property loaderState The state this scope is in.
 */
interface ImageLoaderScope {
    val loaderState: MutableState<ImageLoaderState>
}

/**
 * Renders the inner [content] block if an image was loaded successfully.
 */
@Composable
fun ImageLoaderScope.WithImage(
    content: @Composable (Painter) -> Unit,
) {
    WithInternalScope {
        val state = loaderState.value
        if (state is ImageLoaderState.Image) {
            content(state.painter)
        }
    }
}

/**
 * Renders the inner [content] block while the image is still getting loaded.
 */
@Composable
fun ImageLoaderScope.Placeholder(
    content: @Composable () -> Unit,
) {
    WithInternalScope {
        val state = loaderState.value
        if (state == ImageLoaderState.Loading) {
            content()
        }
    }
}

/**
 * Renders the inner [content] block if loading the image failed.
 */
@Composable
fun ImageLoaderScope.Fallback(
    content: @Composable () -> Unit,
) {
    WithInternalScope {
        val state = loaderState.value
        if (state == ImageLoaderState.Failed) {
            content()
        }
    }
}

@Suppress("LongParameterList")
internal class InternalImageLoaderScope(
    val client: Client,
    val url: String,
    val private: Boolean,
    val connectTimeout: Pair<Long, TimeUnit>,
    val readTimeout: Pair<Long, TimeUnit>,
    val desiredSize: DesiredSize,
    override val loaderState: MutableState<ImageLoaderState> = mutableStateOf(ImageLoaderState.Loading),
) : ImageLoaderScope

@Composable
private fun ImageLoaderScope.WithInternalScope(
    content: @Composable InternalImageLoaderScope.() -> Unit,
) {
    val internalScope = this as InternalImageLoaderScope
    internalScope.content()
}
