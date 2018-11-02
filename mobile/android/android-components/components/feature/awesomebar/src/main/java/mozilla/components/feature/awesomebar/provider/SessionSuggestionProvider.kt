/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.tabs.TabsUseCases

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on the sessions in the
 * [SessionManager] (Open tabs).
 */
class SessionSuggestionProvider(
    private val sessionManager: SessionManager,
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase
) : AwesomeBar.SuggestionProvider {
    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = mutableListOf<AwesomeBar.Suggestion>()

        sessionManager.sessions.forEach { session ->
            if (session.url.contains(text, ignoreCase = true) ||
                session.title.contains(text, ignoreCase = true)
            ) {
                suggestions.add(
                    AwesomeBar.Suggestion(
                        id = "mozac-browser-session:${session.id}",
                        title = session.title,
                        description = session.url,
                        onSuggestionClicked = { selectTabUseCase.invoke(session) }
                    )
                )
            }
        }

        return suggestions
    }
}
