/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.res.Resources
import kotlinx.coroutines.Deferred
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.R
import mozilla.components.feature.tabs.TabsUseCases
import java.util.UUID

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on the sessions in the
 * [SessionManager] (Open tabs).
 */
class SessionSuggestionProvider(
    private val resources: Resources,
    private val sessionManager: SessionManager,
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase,
    private val icons: BrowserIcons? = null,
    private val excludeSelectedSession: Boolean = false
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = mutableListOf<AwesomeBar.Suggestion>()
        val iconRequests: List<Deferred<Icon>?> = sessionManager.sessions.map { icons?.loadIcon(IconRequest(it.url)) }

        sessionManager.sessions.zip(iconRequests) { result, icon ->
            if (result.contains(text) && !result.private && shouldIncludeSelectedSession(result)
            ) {
                suggestions.add(
                        AwesomeBar.Suggestion(
                            provider = this,
                            id = result.id,
                            title = result.title,
                            description = resources.getString(R.string.switch_to_tab_description),
                            icon = icon?.await()?.bitmap,
                            onSuggestionClicked = { selectTabUseCase(result) }
                        )
                )
            }
        }
        return suggestions
    }

    private fun Session.contains(text: String) =
            (url.contains(text, ignoreCase = true) || title.contains(text, ignoreCase = true))

    private fun shouldIncludeSelectedSession(session: Session): Boolean {
        return if (excludeSelectedSession) {
            !isSelectedSession(session)
        } else {
            true
        }
    }

    private fun isSelectedSession(session: Session) = sessionManager.selectedSession?.equals(session) ?: false
}
