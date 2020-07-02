/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.toList
import mozilla.components.support.ktx.kotlinx.coroutines.flow.filterChanged
import org.json.JSONObject

abstract class BaseSearchTelemetry {

    @VisibleForTesting
    internal val providerList = listOf(
        SearchProviderModel(
            name = "google",
            regexp = "^https:\\/\\/www\\.google\\.(?:.+)\\/search",
            queryParam = "q",
            codeParam = "client",
            codePrefixes = listOf("firefox"),
            followOnParams = listOf("oq", "ved", "ei"),
            extraAdServersRegexps = listOf("^https?:\\/\\/www\\.google(?:adservices)?\\.com\\/(?:pagead\\/)?aclk")
        ),
        SearchProviderModel(
            name = "duckduckgo",
            regexp = "^https:\\/\\/duckduckgo\\.com\\/",
            queryParam = "q",
            codeParam = "t",
            codePrefixes = listOf("f"),
            extraAdServersRegexps = listOf(
                "^https:\\/\\/duckduckgo.com\\/y\\.js",
                "^https:\\/\\/www\\.amazon\\.(?:[a-z.]{2,24}).*(?:tag=duckduckgo-)"
            )
        ),
        SearchProviderModel(
            name = "yahoo",
            regexp = "^https:\\/\\/(?:.*)search\\.yahoo\\.com\\/search",
            queryParam = "p"
        ),
        SearchProviderModel(
            name = "baidu",
            regexp = "^https:\\/\\/www\\.baidu\\.com\\/from=844b\\/(?:s|baidu)",
            queryParam = "wd",
            codeParam = "tn",
            codePrefixes = listOf("34046034_", "monline_"),
            followOnParams = listOf("oq")
        ),
        SearchProviderModel(
            name = "bing",
            regexp = "^https:\\/\\/www\\.bing\\.com\\/search",
            queryParam = "q",
            codeParam = "pc",
            codePrefixes = listOf("MOZ", "MZ"),
            followOnCookies = listOf(
                SearchProviderCookie(
                    extraCodeParam = "form",
                    extraCodePrefixes = listOf("QBRE"),
                    host = "www.bing.com",
                    name = "SRCHS",
                    codeParam = "PC",
                    codePrefixes = listOf("MOZ", "MZ")
                )
            ),
            extraAdServersRegexps = listOf(
                "^https:\\/\\/www\\.bing\\.com\\/acli?c?k",
                "^https:\\/\\/www\\.bing\\.com\\/fd\\/ls\\/GLinkPingPost\\.aspx.*acli?c?k"
            )
        )
    )

    abstract fun install(engine: Engine, store: BrowserStore)

    internal fun getProviderForUrl(url: String): SearchProviderModel? =
        providerList.find { provider -> provider.regexp.containsMatchIn(url) }

    internal fun installWebExtension(
        engine: Engine,
        store: BrowserStore,
        extensionInfo: ExtensionInfo
    ) {
        engine.installWebExtension(
            id = extensionInfo.id,
            url = extensionInfo.resourceUrl,
            allowContentMessaging = true,
            onSuccess = { extension ->
                store.flowScoped { flow -> subscribeToUpdates(flow, extension, extensionInfo) }
            },
            onError = { _, throwable ->
                Logger.error("Could not install ${extensionInfo.id} extension", throwable)
            })
    }

    private suspend fun subscribeToUpdates(
        flow: Flow<BrowserState>,
        extension: WebExtension,
        extensionInfo: ExtensionInfo
    ) {
        // Whenever we see a new EngineSession in the store then we register our content message
        // handler if it has not been added yet.
        flow.map { it.tabs }
            .filterChanged { it.engineState.engineSession }
            .collect { state ->
                val engineSession = state.engineState.engineSession ?: return@collect

                if (extension.hasContentMessageHandler(engineSession, extensionInfo.messageId)) {
                    return@collect
                }
                extension.registerContentMessageHandler(
                    engineSession,
                    extensionInfo.messageId,
                    SearchTelemetryMessageHandler()
                )
            }
    }

    protected fun <T> getMessageList(message: JSONObject, key: String): List<T> {
        return message.getJSONArray(key).toList()
    }

    /**
     * This method is used to process any valid json message coming from a web-extension
     */
    @VisibleForTesting
    internal abstract fun processMessage(message: JSONObject)

    @VisibleForTesting
    internal inner class SearchTelemetryMessageHandler : MessageHandler {

        override fun onMessage(message: Any, source: EngineSession?): Any? {
            if (message is JSONObject) {
                processMessage(message)
            } else {
                throw IllegalStateException("Received unexpected message: $message")
            }

            // Needs to return something that is not null and not Unit:
            // https://github.com/mozilla-mobile/android-components/issues/2969
            return ""
        }
    }
}
