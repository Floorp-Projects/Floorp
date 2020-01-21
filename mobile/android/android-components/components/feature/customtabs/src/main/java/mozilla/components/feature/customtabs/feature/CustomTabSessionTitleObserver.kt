/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.feature

import mozilla.components.browser.session.Session
import mozilla.components.concept.toolbar.Toolbar

/**
 * Sets the title of the custom tab toolbar based on the session title and URL.
 */
class CustomTabSessionTitleObserver(
    private val toolbar: Toolbar
) : Session.Observer {
    private var receivedTitle = false

    override fun onUrlChanged(session: Session, url: String) {
        // If we showed a title once in a custom tab then we are going to continue displaying
        // a title (to avoid the layout bouncing around). However if no title is available then
        // we just use the URL.
        if (receivedTitle && session.title.isEmpty()) {
            toolbar.title = url
        }
    }

    override fun onTitleChanged(session: Session, title: String) {
        if (title.isNotEmpty()) {
            toolbar.title = title
            receivedTitle = true
        } else if (receivedTitle) {
            // See comment in OnUrlChanged().
            toolbar.title = session.url
        }
    }
}
