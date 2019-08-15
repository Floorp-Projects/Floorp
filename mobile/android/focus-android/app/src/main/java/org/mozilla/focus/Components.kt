/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import android.content.Context
import android.util.AttributeSet
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.search.provider.AssetsSearchEngineProvider
import mozilla.components.browser.search.provider.localization.LocaleSearchLocalizationProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.utils.EngineVersion
import org.json.JSONObject
import org.mozilla.focus.search.BingSearchEngineFilter
import org.mozilla.focus.search.CustomSearchEngineProvider
import org.mozilla.focus.search.HiddenSearchEngineFilter

/**
 * Helper object for lazily initializing components.
 */
class Components {
    val sessionManager by lazy {
        SessionManager(DummyEngine()).apply {
            register(SessionSetupObserver())
        }
    }

    val searchEngineManager by lazy {
        val assetsProvider = AssetsSearchEngineProvider(
                LocaleSearchLocalizationProvider(),
                filters = listOf(BingSearchEngineFilter(), HiddenSearchEngineFilter()),
                additionalIdentifiers = listOf("ddg"))

        val customProvider = CustomSearchEngineProvider()

        SearchEngineManager(listOf(assetsProvider, customProvider))
    }
}

/**
 * We are not using an "Engine" implementation yet. Therefore we create this dummy that we pass to
 * the <code>SessionManager</code> for now.
 */
private class DummyEngine : Engine {
    override val version: EngineVersion = EngineVersion(1, 0, 0)
    override val settings: Settings = DefaultSettings()

    override fun createSession(private: Boolean): EngineSession {
        throw NotImplementedError()
    }

    override fun createSessionState(json: JSONObject): EngineSessionState {
        throw NotImplementedError()
    }

    override fun createView(context: Context, attrs: AttributeSet?): EngineView {
        throw NotImplementedError()
    }

    override fun name(): String {
        throw NotImplementedError()
    }

    override fun speculativeConnect(url: String) {
        throw NotImplementedError()
    }
}

/**
 * This is a workaround for setting up the Session object. Once we use browser-engine we can just setup the Engine to
 * use specific default settings. For now we need to manually update every created Session model to reflect what Focus
 * expects as default.
 */
class SessionSetupObserver : SessionManager.Observer {
    override fun onSessionAdded(session: Session) {
        // Tracking protection is enabled by default for every tab. Instead of setting up the engine we modify Session
        // here and IWebView will get its default behavior from Session.
        session.trackerBlockingEnabled = true
    }
}
