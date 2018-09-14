/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.samples.browser

import android.content.Context
import android.widget.Toast
import kotlinx.coroutines.experimental.async
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuCheckbox
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.DefaultSessionStorage
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionIntentProcessor
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TextSearchHandler
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.samples.browser.request.SampleRequestInterceptor

/**
 * Helper class for lazily instantiating components needed by the application.
 */
class Components(private val applicationContext: Context) : DefaultComponents(applicationContext){
    override val engine: Engine by lazy {
        val defaultSettings = DefaultSettings().apply {
            requestInterceptor = SampleRequestInterceptor()
        }

        val runtime = GeckoRuntime.getDefault(applicationContext)
        GeckoEngine(runtime, defaultSettings)
    }
}
