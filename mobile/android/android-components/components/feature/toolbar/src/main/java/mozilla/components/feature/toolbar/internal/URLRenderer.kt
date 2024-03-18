/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar.internal

import android.text.SpannableStringBuilder
import android.text.SpannableStringBuilder.SPAN_INCLUSIVE_INCLUSIVE
import android.text.style.ForegroundColorSpan
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.trySendBlocking
import kotlinx.coroutines.launch
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.ToolbarFeature

/**
 * Asynchronous URL renderer.
 *
 * This "renderer" will create a (potentially) colored URL (using spans) in a coroutine and set it on the [Toolbar].
 */
internal class URLRenderer(
    private val toolbar: Toolbar,
    private val configuration: ToolbarFeature.UrlRenderConfiguration?,
) {
    private val scope = CoroutineScope(Dispatchers.Main)

    @VisibleForTesting internal var job: Job? = null

    @VisibleForTesting internal val channel = Channel<String>(capacity = Channel.CONFLATED)

    /**
     * Starts this renderer which will listen for incoming URLs to render.
     */
    fun start() {
        job = scope.launch {
            for (url in channel) {
                updateUrl(url)
            }
        }
    }

    /**
     * Stops this renderer.
     */
    fun stop() {
        job?.cancel()
    }

    /**
     * Posts this [url] to the renderer.
     */
    fun post(url: String) {
        try {
            channel.trySendBlocking(url)
        } catch (e: InterruptedException) {
            // Ignore
        }
    }

    @VisibleForTesting
    internal suspend fun updateUrl(url: String) {
        if (url.isEmpty() || configuration == null) {
            toolbar.url = url
            return
        }

        toolbar.url = when (configuration.renderStyle) {
            // Display only the URL, uncolored
            ToolbarFeature.RenderStyle.RegistrableDomain -> {
                val host = url.toUri().host?.ifEmpty { null }
                host?.let { getRegistrableDomain(host, configuration) } ?: url
            }
            // Display the registrableDomain with color and URL with another color
            ToolbarFeature.RenderStyle.ColoredUrl -> SpannableStringBuilder(url).apply {
                color(configuration.urlColor)
                colorRegistrableDomain(configuration)
            }
            // Display the full URL, uncolored
            ToolbarFeature.RenderStyle.UncoloredUrl -> url
        }
    }
}

private suspend fun getRegistrableDomain(host: String, configuration: ToolbarFeature.UrlRenderConfiguration) =
    configuration.publicSuffixList.getPublicSuffixPlusOne(host).await()

private suspend fun SpannableStringBuilder.colorRegistrableDomain(
    configuration: ToolbarFeature.UrlRenderConfiguration,
) {
    val url = toString()
    val host = url.toUri().host ?: return

    val registrableDomain = configuration
        .publicSuffixList
        .getPublicSuffixPlusOne(host)
        .await() ?: return

    val index = url.indexOf(registrableDomain)
    if (index == -1) {
        return
    }

    setSpan(
        ForegroundColorSpan(configuration.registrableDomainColor),
        index,
        index + registrableDomain.length,
        SPAN_INCLUSIVE_INCLUSIVE,
    )
}

private fun SpannableStringBuilder.color(@ColorInt urlColor: Int?) {
    urlColor ?: return

    setSpan(
        ForegroundColorSpan(urlColor),
        0,
        length,
        SPAN_INCLUSIVE_INCLUSIVE,
    )
}
