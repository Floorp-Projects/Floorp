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
import mozilla.components.browser.icons.extension.IconSessionObserver
import mozilla.components.browser.icons.generator.DefaultIconGenerator
import mozilla.components.browser.icons.generator.IconGenerator
import mozilla.components.browser.icons.loader.DataUriIconLoader
import mozilla.components.browser.icons.loader.DiskIconLoader
import mozilla.components.browser.icons.loader.HttpIconLoader
import mozilla.components.browser.icons.loader.IconLoader
import mozilla.components.browser.icons.loader.MemoryIconLoader
import mozilla.components.browser.icons.pipeline.IconResourceComparator
import mozilla.components.browser.icons.preparer.DiskIconPreparer
import mozilla.components.browser.icons.preparer.IconPreprarer
import mozilla.components.browser.icons.preparer.MemoryIconPreparer
import mozilla.components.browser.icons.preparer.TippyTopIconPreparer
import mozilla.components.browser.icons.processor.DiskIconProcessor
import mozilla.components.browser.icons.processor.IconProcessor
import mozilla.components.browser.icons.processor.MemoryIconProcessor
import mozilla.components.browser.icons.utils.DiskCache
import mozilla.components.browser.icons.utils.MemoryCache
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.utils.AllSessionsObserver
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.util.dpToPx
import java.util.concurrent.Executors

private const val MAXIMUM_SCALE_FACTOR = 2.0f

// Number of worker threads we are using internally.
private const val THREADS = 3

internal val sharedMemoryCache = MemoryCache()
internal val sharedDiskCache = DiskCache()

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
        TippyTopIconPreparer(context.assets),
        MemoryIconPreparer(sharedMemoryCache),
        DiskIconPreparer(sharedDiskCache)
    ),
    private val loaders: List<IconLoader> = listOf(
        MemoryIconLoader(sharedMemoryCache),
        DiskIconLoader(sharedDiskCache),
        HttpIconLoader(httpClient),
        DataUriIconLoader()
    ),
    private val decoders: List<IconDecoder> = listOf(
        AndroidIconDecoder(),
        ICOIconDecoder()
    ),
    private val processors: List<IconProcessor> = listOf(
        MemoryIconProcessor(sharedMemoryCache),
        DiskIconProcessor(sharedDiskCache)
    ),
    jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS).asCoroutineDispatcher()
) {
    private val logger = Logger("BrowserIcons")
    private val maximumSize = context.resources.getDimensionPixelSize(R.dimen.mozac_browser_icons_maximum_size)
    private val scope = CoroutineScope(jobDispatcher)

    /**
     * Asynchronously loads an [Icon] for the given [IconRequest].
     */
    fun loadIcon(request: IconRequest): Deferred<Icon> = scope.async {
        loadIconInternal(request).also { loadedIcon ->
            logger.debug("Loaded icon (source = ${loadedIcon.source}): ${request.url}")
        }
    }

    private fun loadIconInternal(initialRequest: IconRequest): Icon {
        val desiredSize = DesiredSize(
            targetSize = initialRequest.size.value.dpToPx(context.resources.displayMetrics),
            maxSize = maximumSize,
            maxScaleFactor = MAXIMUM_SCALE_FACTOR
        )

        // (1) First prepare the request.
        val request = prepare(context, preparers, initialRequest)

        // (2) Then try to load an icon.
        val (result, resource) = load(context, request, loaders)

        val icon = when (result) {
            IconLoader.Result.NoResult -> return generator.generate(context, request)

            is IconLoader.Result.BitmapResult -> Icon(result.bitmap, source = result.source)

            is IconLoader.Result.BytesResult ->
                decode(result.bytes, decoders, desiredSize)?.let { bitmap ->
                    Icon(bitmap, source = result.source)
                } ?: return generator.generate(context, request)
        }

        // (3) Finally process the icon.
        return process(context, processors, request, resource, icon, desiredSize)
            ?: generator.generate(context, request)
    }

    /**
     * Installs the "icons" extension in the engine in order to dynamically load icons for loaded websites.
     */
    fun install(engine: Engine, sessionManager: SessionManager) {
        engine.installWebExtension(
            id = "mozacBrowserIcons",
            url = "resource://android/assets/extensions/browser-icons/",
            allowContentMessaging = true,
            onSuccess = { extension ->
                Logger.debug("Installed browser-icons extension")

                AllSessionsObserver(sessionManager, IconSessionObserver(this, sessionManager, extension))
                    .start()
            },
            onError = { _, throwable ->
                Logger.error("Could not install browser-icons extension", throwable)
            })
    }
}

private fun prepare(context: Context, preparers: List<IconPreprarer>, request: IconRequest): IconRequest =
    preparers.fold(request) { preparedRequest, preparer ->
        preparer.prepare(context, preparedRequest)
    }

private fun load(
    context: Context,
    request: IconRequest,
    loaders: List<IconLoader>
): Pair<IconLoader.Result, IconRequest.Resource?> {
    // We are just looping over the resources here. We need to rank them first to try the best icon first.
    // https://github.com/mozilla-mobile/android-components/issues/2048
    request.resources.toSortedSet(IconResourceComparator).forEach { resource ->
        loaders.forEach { loader ->
            val result = loader.load(context, request, resource)

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
    desiredSize: DesiredSize
): Bitmap? {
    decoders.forEach { decoder ->
        val bitmap = decoder.decode(data, desiredSize)

        if (bitmap != null) {
            return bitmap
        }
    }

    return null
}

@Suppress("LongParameterList")
private fun process(
    context: Context,
    processors: List<IconProcessor>,
    request: IconRequest,
    resource: IconRequest.Resource?,
    icon: Icon?,
    desiredSize: DesiredSize
): Icon? =
    processors.fold(icon) { processedIcon, processor ->
        if (processedIcon == null) return null
        processor.process(context, request, resource, processedIcon, desiredSize)
    }
