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
import mozilla.components.browser.icons.loader.DataUriIconLoader
import mozilla.components.browser.icons.loader.HttpIconLoader
import mozilla.components.browser.icons.loader.IconLoader
import mozilla.components.browser.icons.loader.MemoryIconLoader
import mozilla.components.browser.icons.pipeline.IconResourceComparator
import mozilla.components.browser.icons.preparer.IconPreprarer
import mozilla.components.browser.icons.preparer.MemoryIconPreparer
import mozilla.components.browser.icons.processor.IconProcessor
import mozilla.components.browser.icons.processor.MemoryIconProcessor
import mozilla.components.browser.icons.utils.MemoryCache
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

internal val sharedMemoryCache = MemoryCache()

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
    private val preparers: List<IconPreprarer> = listOf(
        MemoryIconPreparer(sharedMemoryCache)
    ),
    private val loaders: List<IconLoader> = listOf(
        MemoryIconLoader(sharedMemoryCache),
        HttpIconLoader(httpClient),
        DataUriIconLoader()
    ),
    private val decoders: List<IconDecoder> = listOf(
        AndroidIconDecoder(),
        ICOIconDecoder()
    ),
    private val processors: List<IconProcessor> = listOf(
        MemoryIconProcessor(sharedMemoryCache)
    ),
    jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS).asCoroutineDispatcher()
) {
    private val maximumSize = context.resources.pxToDp(MAXIMUM_SIZE_DP)
    private val scope = CoroutineScope(jobDispatcher)

    /**
     * Asynchronously loads an [Icon] for the given [IconRequest].
     */
    fun loadIcon(initialRequest: IconRequest): Deferred<Icon> = scope.async {
        val targetSize = context.resources.pxToDp(initialRequest.size.value)

        // (1) First prepare the request.
        val request = prepare(preparers, initialRequest)

        // (2) Then try to load an icon.
        val (result, resource) = load(request, loaders)

        val icon = when (result) {
            IconLoader.Result.NoResult -> return@async generator.generate(context, request)

            is IconLoader.Result.BitmapResult -> Icon(result.bitmap, source = result.source)

            is IconLoader.Result.BytesResult ->
                decode(result.bytes, decoders, targetSize, maximumSize)?.let { bitmap ->
                    Icon(bitmap, source = result.source)
                } ?: return@async generator.generate(context, request)
        }

        // (3) Finally process the icon.
        process(processors, request, resource, icon)
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

private fun prepare(preparers: List<IconPreprarer>, request: IconRequest): IconRequest {
    var preparedRequest: IconRequest = request

    preparers.forEach { preparer ->
        preparedRequest = preparer.prepare(preparedRequest)
    }

    return preparedRequest
}

private fun load(request: IconRequest, loaders: List<IconLoader>): Pair<IconLoader.Result, IconRequest.Resource?> {
    // We are just looping over the resources here. We need to rank them first to try the best icon first.
    // https://github.com/mozilla-mobile/android-components/issues/2048
    request.resources.toSortedSet(IconResourceComparator).forEach { resource ->
        loaders.forEach { loader ->
            val result = loader.load(request, resource)

            if (result != IconLoader.Result.NoResult) {
                return Pair(result, resource)
            }
        }
    }

    return Pair(IconLoader.Result.NoResult, null)
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

private fun process(
    processors: List<IconProcessor>,
    request: IconRequest,
    resource: IconRequest.Resource?,
    icon: Icon
): Icon {
    var processedIcon = icon

    processors.forEach { processor ->
        processedIcon = processor.process(request, resource, processedIcon)
    }

    return processedIcon
}
