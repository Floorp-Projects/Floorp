/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

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
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action.INTERACTION
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlinx.coroutines.flow.filterChanged
import org.json.JSONObject

/**
 * Main configuration and functionality for tracking ads / web searches with specific providers.
 */
abstract class BaseSearchTelemetry {

    /**
     * Install the web extensions that this functionality is based on and start listening for updates.
     */
    abstract fun install(engine: Engine, store: BrowserStore)

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    internal fun getProviderForUrl(url: String): SearchProviderModel? =
        providerList.find { provider -> provider.regexp.containsMatchIn(url) }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    internal fun installWebExtension(
        engine: Engine,
        store: BrowserStore,
        extensionInfo: ExtensionInfo,
    ) {
        engine.installWebExtension(
            id = extensionInfo.id,
            url = extensionInfo.resourceUrl,
            onSuccess = { extension ->
                store.flowScoped { flow -> subscribeToUpdates(flow, extension, extensionInfo) }
            },
            onError = { _, throwable ->
                Logger.error("Could not install ${extensionInfo.id} extension", throwable)
            },
        )
    }

    protected fun emitFact(
        event: String,
        value: String,
        metadata: Map<String, Any>? = null,
    ) {
        Fact(
            Component.FEATURE_SEARCH,
            INTERACTION,
            event,
            value,
            metadata,
        ).collect()
    }

    protected sealed class Action

    @VisibleForTesting
    internal val providerList = listOf(
        SearchProviderModel(
            name = "google",
            regexp = "^https:\\/\\/www\\.google\\.(?:.+)\\/search",
            queryParam = "q",
            codeParam = "client",
            codePrefixes = listOf("firefox"),
            followOnParams = listOf("oq", "ved", "ei"),
            extraAdServersRegexps = listOf("^https?:\\/\\/www\\.google(?:adservices)?\\.com\\/(?:pagead\\/)?aclk"),
        ),
        SearchProviderModel(
            name = "duckduckgo",
            regexp = "^https:\\/\\/duckduckgo\\.com\\/",
            queryParam = "q",
            codeParam = "t",
            codePrefixes = listOf("f"),
            extraAdServersRegexps = listOf(
                "^https:\\/\\/duckduckgo.com\\/y\\.js",
                "^https:\\/\\/www\\.amazon\\.(?:[a-z.]{2,24}).*(?:tag=duckduckgo-)",
            ),
        ),
        SearchProviderModel(
            name = "yahoo",
            regexp = "^https:\\/\\/(?:.*)search\\.yahoo\\.com\\/search",
            queryParam = "p",
        ),
        SearchProviderModel(
            name = "baidu",
            regexp = "^https:\\/\\/m\\.baidu\\.com(?:.*)\\/s",
            queryParam = "word",
            codeParam = "from",
            codePrefixes = listOf("1000969a"),
            followOnParams = listOf("oq"),
            extraAdServersRegexps = listOf(
                "^https?://www\\.baidu\\.com/baidu\\.php?",
                "^https?://m\\.baidu\\.com/baidu\\.php?",
            ),
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
                    codePrefixes = listOf("MOZ", "MZ"),
                ),
            ),
            extraAdServersRegexps = listOf(
                "^https:\\/\\/www\\.bing\\.com\\/acli?c?k",
                "^https:\\/\\/www\\.bing\\.com\\/fd\\/ls\\/GLinkPingPost\\.aspx.*acli?c?k",
            ),
        ),
    )

    private suspend fun subscribeToUpdates(
        flow: Flow<BrowserState>,
        extension: WebExtension,
        extensionInfo: ExtensionInfo,
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
                    SearchTelemetryMessageHandler(),
                )
            }
    }

    /**
     * This method is used to process any valid json message coming from a web-extension.
     */
    @VisibleForTesting
    internal abstract fun processMessage(message: JSONObject)

    @VisibleForTesting
    internal inner class SearchTelemetryMessageHandler : MessageHandler {

        @Throws(IllegalStateException::class)
        override fun onMessage(message: Any, source: EngineSession?): Any {
            if (message is JSONObject) {
                processMessage(message)
            } else {
                throw IllegalStateException("Received unexpected message: $message")
            }

            return Unit
        }
    }
}
