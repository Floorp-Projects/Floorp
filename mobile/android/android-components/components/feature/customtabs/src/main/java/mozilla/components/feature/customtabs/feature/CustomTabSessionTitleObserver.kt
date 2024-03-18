/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.feature

import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.concept.toolbar.Toolbar

/**
 * Sets the title of the custom tab toolbar based on the session title and URL.
 */
class CustomTabSessionTitleObserver(
    private val toolbar: Toolbar,
) {
    private var url: String? = null
    private var title: String? = null
    private var showedTitle = false

    internal fun onTab(tab: CustomTabSessionState) {
        if (tab.content.title != title) {
            onTitleChanged(tab)
            title = tab.content.title
        }

        if (tab.content.url != url) {
            onUrlChanged(tab)
            url = tab.content.url
        }
    }

    private fun onUrlChanged(tab: CustomTabSessionState) {
        // If we showed a title once in a custom tab then we are going to continue displaying
        // a title (to avoid the layout bouncing around). However if no title is available then
        // we just use the URL.
        if (showedTitle && tab.content.title.isEmpty()) {
            toolbar.title = tab.content.url
        }
    }

    private fun onTitleChanged(tab: CustomTabSessionState) {
        if (tab.content.title.isNotEmpty()) {
            toolbar.title = tab.content.title
            showedTitle = true
        } else if (showedTitle) {
            // See comment in OnUrlChanged().
            toolbar.title = tab.content.url
        }
    }
}
