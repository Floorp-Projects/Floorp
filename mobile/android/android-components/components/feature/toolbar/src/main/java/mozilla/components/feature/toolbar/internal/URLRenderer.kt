/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar.internal

import android.text.SpannableStringBuilder
import android.text.style.ForegroundColorSpan
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.sendBlocking
import kotlinx.coroutines.launch
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.support.ktx.kotlin.toUri

/**
 * Asynchronous URL renderer.
 *
 * This "renderer" will create a (potentially) colored URL (using spans) in a coroutine and set it on the [Toolbar].
 */
internal class URLRenderer(
    private val toolbar: Toolbar,
    private val configuration: ToolbarFeature.UrlRenderConfiguration?
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
        channel.sendBlocking(url)
    }

    @VisibleForTesting
    internal suspend fun updateUrl(url: String) {
        if (url.isEmpty() || configuration == null) {
            setUncoloredUrl(url)
            return
        }

        when (configuration.renderStyle) {
            ToolbarFeature.RenderStyle.UncoloredUrl -> setUncoloredUrl(url)
            ToolbarFeature.RenderStyle.ColoredUrl -> setColoredUrl(url, configuration)
            ToolbarFeature.RenderStyle.RegistrableDomain -> setRegistrableDomainUrl(url)
        }
    }

    private suspend fun setRegistrableDomainUrl(url: String) {
        val host = url.toUri().host

        if (!host.isNullOrEmpty()) {
            toolbar.url = getRegistrableDomain(host) ?: url
        } else {
            toolbar.url = url
        }
    }

    private suspend fun setColoredUrl(url: String, configuration: ToolbarFeature.UrlRenderConfiguration) {
        val builder = SpannableStringBuilder(url)

        builder.color(configuration)
        builder.colorRegistrableDomain(configuration)

        toolbar.url = builder
    }

    private fun setUncoloredUrl(url: String) {
        toolbar.url = url
    }

    private suspend fun getRegistrableDomain(host: String): CharSequence? {
        return configuration?.publicSuffixList?.getPublicSuffixPlusOne(host)?.await()
    }
}

private suspend fun SpannableStringBuilder.colorRegistrableDomain(
    configuration: ToolbarFeature.UrlRenderConfiguration
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
        SpannableStringBuilder.SPAN_INCLUSIVE_INCLUSIVE)
}

private fun SpannableStringBuilder.color(
    configuration: ToolbarFeature.UrlRenderConfiguration
) {
    if (configuration.urlColor == null) {
        return
    }

    setSpan(
        ForegroundColorSpan(
            configuration.urlColor),
        0,
        length,
        SpannableStringBuilder.SPAN_INCLUSIVE_INCLUSIVE)
}
