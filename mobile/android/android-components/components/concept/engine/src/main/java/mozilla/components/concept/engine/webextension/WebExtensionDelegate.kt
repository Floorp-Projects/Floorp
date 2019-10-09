/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import mozilla.components.concept.engine.EngineSession

/**
 * Notifies applications or other components of engine events related to web
 * extensions e.g. an extension was installed, or an extension wants to open
 * a new tab.
 */
interface WebExtensionDelegate {

    /**
     * Invoked when a web extension was installed successfully.
     *
     * @param webExtension The installed extension.
     */
    fun onInstalled(webExtension: WebExtension) = Unit

    /**
     * Invoked when a web extension attempts to open a new tab via
     * browser.tabs.create. Note that browser.tabs.remove is currently
     * not supported:
     * https://github.com/mozilla-mobile/android-components/issues/4682
     *
     * @param webExtension An instance of [WebExtension] or null if extension
     * was not registered with the engine.
     * @param url the target url to be loaded in a new tab.
     * @param engineSession an instance of engine session to open a new tab with.
     */
    fun onNewTab(webExtension: WebExtension?, url: String, engineSession: EngineSession) = Unit
}
