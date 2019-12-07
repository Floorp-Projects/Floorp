/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.remotetabs

import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions for remote tabs
 * based on [RemoteTabsFeature].
 */
@ExperimentalCoroutinesApi
class RemoteTabsStorageSuggestionProvider(
    private val remoteTabs: RemoteTabsFeature,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null
) : AwesomeBar.SuggestionProvider {

    override val id: String = UUID.randomUUID().toString()

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        val results = mutableListOf<ClientTabPair>()
        for ((client, tabs) in remoteTabs.getRemoteTabs()) {
            for (tab in tabs) {
                val activeTabEntry = tab.active()
                // This is a fairly naive match implementation, but this is what we do on Desktop 🤷.
                if (activeTabEntry.url.contains(text, ignoreCase = true) ||
                    activeTabEntry.title.contains(text, ignoreCase = true)) {
                    results.add(ClientTabPair(
                        clientName = client.displayName,
                        tab = activeTabEntry,
                        lastUsed = tab.lastUsed
                    ))
                }
            }
        }
        return results.sortedByDescending { it.lastUsed }.into()
    }

    /**
     * Expects list of BookmarkNode to be specifically of bookmarks (e.g. nodes with a url).
     */
    private suspend fun List<ClientTabPair>.into(): List<AwesomeBar.Suggestion> {
        val iconRequests = this.map { client ->
            client.tab.iconUrl?.let { iconUrl -> icons?.loadIcon(IconRequest(iconUrl)) }
        }

        return this.zip(iconRequests) { result, icon ->
            AwesomeBar.Suggestion(
                provider = this@RemoteTabsStorageSuggestionProvider,
                icon = icon?.await()?.bitmap,
                title = result.tab.title,
                description = result.clientName,
                onSuggestionClicked = { loadUrlUseCase.invoke(result.tab.url) }
            )
        }
    }
}

private data class ClientTabPair(val clientName: String, val tab: TabEntry, val lastUsed: Long)
