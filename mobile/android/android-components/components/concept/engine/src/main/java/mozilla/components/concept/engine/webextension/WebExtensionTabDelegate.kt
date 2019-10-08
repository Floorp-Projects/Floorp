/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import mozilla.components.concept.engine.EngineSession

/**
 * Notifies applications / other components that a web extension wants to open a new tab via
 * browser.tabs.create. Note that browser.tabs.remove is currently not supported:
 * https://github.com/mozilla-mobile/android-components/issues/4682
 */
interface WebExtensionTabDelegate {

    /**
     * Invoked when a web extension opens a new tab.
     *
     * @param webExtension An instance of [WebExtension] or null if extension was not registered with the engine.
     * @param url the target url to be loaded in a new tab.
     * @param engineSession an instance of engine session to open a new tab with.
     */
    fun onNewTab(webExtension: WebExtension?, url: String, engineSession: EngineSession) = Unit
}
