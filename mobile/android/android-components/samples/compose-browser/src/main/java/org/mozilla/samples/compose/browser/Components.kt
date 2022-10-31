/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser

import android.content.Context
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.search.middleware.SearchMiddleware
import mozilla.components.feature.search.region.RegionMiddleware
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.location.LocationService
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.samples.compose.browser.app.AppStore

/**
 * Global components of the sample browser.
 */
class Components(
    context: Context,
) {
    private val runtime by lazy { GeckoRuntime.create(context) }

    val engine: Engine by lazy { GeckoEngine(context, runtime = runtime) }
    val client: Client by lazy { GeckoViewFetchClient(context, runtime = runtime) }

    val store: BrowserStore by lazy {
        BrowserStore(
            middleware = listOf(
                RegionMiddleware(context, locationService),
                SearchMiddleware(context),
            ) + EngineMiddleware.create(engine),
        )
    }

    val appStore: AppStore by lazy { AppStore() }

    val sessionUseCases by lazy { SessionUseCases(store) }
    val tabsUseCases by lazy { TabsUseCases(store) }
    val searchUseCases by lazy { SearchUseCases(store, tabsUseCases, sessionUseCases) }

    val locationService by lazy { LocationService.default() }
}

/**
 * Returns the global [Components] object from within a `@Composable` context.
 */
@Composable
fun components(): Components {
    return (LocalContext.current.applicationContext as BrowserApplication).components
}
