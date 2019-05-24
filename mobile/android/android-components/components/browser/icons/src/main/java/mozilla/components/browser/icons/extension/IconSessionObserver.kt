/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.utils.AllSessionsObserver
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.WebExtension

private const val EXTENSION_MESSAGING_NAME = "MozacBrowserIcons"

/**
 * [Session.Observer] implementation that will setup the content message handler for communicating with the Web
 * Extension for [Session] instances that started loading something.
 *
 * We only setup the handler for [Session]s that started loading something in order to avoid creating an [EngineSession]
 * for all [Session]s - breaking the lazy initialization.
 */
internal class IconSessionObserver(
    private val icons: BrowserIcons,
    private val sessionManager: SessionManager,
    private val extension: WebExtension
) : AllSessionsObserver.Observer {
    override fun onLoadingStateChanged(session: Session, loading: Boolean) {
        if (loading) {
            registerMessageHandler(session)
        }
    }

    private fun registerMessageHandler(session: Session) {
        val engineSession = sessionManager.getOrCreateEngineSession(session)

        if (extension.hasContentMessageHandler(engineSession, EXTENSION_MESSAGING_NAME)) {
            return
        }

        val handler = IconMessageHandler(session, icons)
        extension.registerContentMessageHandler(engineSession, EXTENSION_MESSAGING_NAME, handler)
    }
}
