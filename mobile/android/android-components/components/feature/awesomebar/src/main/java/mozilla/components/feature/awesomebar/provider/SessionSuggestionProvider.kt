/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.internal.loadLambda
import mozilla.components.feature.tabs.TabsUseCases
import java.util.UUID

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on the sessions in the
 * [SessionManager] (Open tabs).
 */
class SessionSuggestionProvider(
    private val sessionManager: SessionManager,
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase,
    private val icons: BrowserIcons? = null
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = mutableListOf<AwesomeBar.Suggestion>()

        sessionManager.sessions.forEach { session ->
            if ((session.url.contains(text, ignoreCase = true) ||
                    session.title.contains(text, ignoreCase = true)) && !session.private
            ) {
                suggestions.add(
                    AwesomeBar.Suggestion(
                        provider = this,
                        id = session.id,
                        title = session.title,
                        description = session.url,
                        icon = icons.loadLambda(session.url),
                        onSuggestionClicked = { selectTabUseCase(session) }
                    )
                )
            }
        }

        return suggestions
    }
}
