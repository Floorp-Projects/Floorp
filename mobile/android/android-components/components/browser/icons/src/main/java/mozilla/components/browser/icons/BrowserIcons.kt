/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import android.content.Context
import android.graphics.Bitmap
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import mozilla.components.browser.icons.decoder.AndroidIconDecoder
import mozilla.components.browser.icons.decoder.ICOIconDecoder
import mozilla.components.browser.icons.decoder.IconDecoder
import mozilla.components.browser.icons.generator.DefaultIconGenerator
import mozilla.components.browser.icons.generator.IconGenerator
import mozilla.components.browser.icons.loader.HttpIconLoader
import mozilla.components.browser.icons.loader.IconLoader
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.fetch.Client
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.res.pxToDp
import java.util.concurrent.Executors

private const val MAXIMUM_SIZE_DP = 64
private const val MAXIMUM_SCALE_FACTOR = 2.0f

// Number of worker threads we are using internally.
private const val THREADS = 3

/**
 * Entry point for loading icons for websites.
 *
 * @param generator The [IconGenerator] to generate an icon if no icon could be loaded.
 * @param decoders List of [IconDecoder] instances to use when decoding a loaded icon into a [android.graphics.Bitmap].
 */
class BrowserIcons(
    private val context: Context,
    private val httpClient: Client,
    private val generator: IconGenerator = DefaultIconGenerator(context),
    private val loaders: List<IconLoader> = listOf(
        HttpIconLoader(httpClient)
    ),
    private val decoders: List<IconDecoder> = listOf(
        AndroidIconDecoder(),
        ICOIconDecoder()
    ),
    jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS).asCoroutineDispatcher()
) {
    private val maximumSize = context.resources.pxToDp(MAXIMUM_SIZE_DP)
    private val scope = CoroutineScope(jobDispatcher)

    /**
     * Asynchronously loads an [Icon] for the given [IconRequest].
     */
    fun loadIcon(request: IconRequest): Deferred<Icon> = scope.async {
        val targetSize = context.resources.pxToDp(request.size.value)

        // (1) First try to load an icon.
        val (data, source) = load(request, loaders)
            ?: return@async generator.generate(context, request)

        // (2) Then try to decode it
        val bitmap = decode(data, decoders, targetSize, maximumSize)
            ?: return@async generator.generate(context, request)

        return@async Icon(bitmap, source = source)
    }

    /**
     * Installs the "icons" extension in the engine in order to dynamically load icons for loaded websites.
     */
    fun install(engine: Engine) {
        engine.installWebExtension(
            WebExtension(
                id = "browser-icons",
                url = "resource://android/assets/extensions/browser-icons/"
            ),
            onSuccess = {
                Logger.debug("Installed browser-icons extension")
            },
            onError = { _, throwable ->
                Logger.error("Could not install browser-icons extension", throwable)
            })
    }
}

private fun load(request: IconRequest, loaders: List<IconLoader>): Pair<ByteArray, Icon.Source>? {
    // We are just looping over the resources here. We need to rank them first to try the best icon first.
    // https://github.com/mozilla-mobile/android-components/issues/2048
    request.resources.forEach { resource ->
        loaders.forEach { loader ->
            val data = loader.load(request, resource)
            if (data != null) {
                return Pair(data, loader.source)
            }
        }
    }

    return null
}

private fun decode(
    data: ByteArray,
    decoders: List<IconDecoder>,
    targetSize: Int,
    maximumSize: Int
): Bitmap? {
    decoders.forEach { decoder ->
        val bitmap = decoder.decode(
            data,
            targetSize = targetSize,
            maxSize = maximumSize,
            maxScaleFactor = MAXIMUM_SCALE_FACTOR)

        if (bitmap != null) {
            return bitmap
        }
    }

    return null
}
