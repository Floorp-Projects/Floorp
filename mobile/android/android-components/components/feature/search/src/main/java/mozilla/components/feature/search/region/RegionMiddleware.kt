/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.region

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.InitAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.service.location.LocationService

/**
 * [Middleware] implementation for updating the [RegionState] using the provided [LocationService].
 */
class RegionMiddleware(
    context: Context,
    locationService: LocationService,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : Middleware<BrowserState, BrowserAction> {
    @VisibleForTesting
    internal var regionManager = RegionManager(context, locationService, dispatcher = ioDispatcher)

    @VisibleForTesting
    @Volatile
    internal var updateJob: Job? = null

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        if (action is InitAction) {
            updateJob = determineRegion(context.store)
        }

        next(action)
    }

    @OptIn(DelicateCoroutinesApi::class)
    private fun determineRegion(
        store: Store<BrowserState, BrowserAction>,
    ) = GlobalScope.launch(ioDispatcher) {
        // Get the region state from the RegionManager. If there's none then dispatch the default
        // region to be used.
        val region = regionManager.region()
        if (region != null) {
            store.dispatch(SearchAction.SetRegionAction(region))
        } else {
            store.dispatch(SearchAction.SetRegionAction(RegionState.Default))
        }

        // Ask the RegionManager to perform an update. If the "home" region changed then it will
        // return a new RegionState.
        val update = regionManager.update()
        if (update != null) {
            store.dispatch(SearchAction.SetRegionAction(update))
        }
    }
}
