/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContainerAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareStore
import kotlin.coroutines.CoroutineContext

/**
 * [Middleware] implementation for handling [ContainerAction] and syncing the containers in
 * [BrowserState.containers] with the [ContainerStorage].
 */
class ContainerMiddleware(
    applicationContext: Context,
    coroutineContext: CoroutineContext = Dispatchers.IO
) : Middleware<BrowserState, BrowserAction> {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val containerStorage: ContainerStorage = ContainerStorage(applicationContext)
    private var scope = CoroutineScope(coroutineContext)

    override fun invoke(
        store: MiddlewareStore<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        when (action) {
            is ContainerAction.AddContainerAction -> {
                scope.launch {
                    containerStorage.addContainer(
                        contextId = action.container.contextId,
                        name = action.container.name,
                        color = action.container.color,
                        icon = action.container.icon
                    )
                }
            }
            is ContainerAction.InitializeContainerState -> {
                scope.launch {
                    containerStorage.getContainers().collect { containers ->
                        store.dispatch(ContainerAction.AddContainersAction(containers))
                    }
                }
            }
            is ContainerAction.RemoveContainerAction -> {
                scope.launch {
                    store.state.containers[action.contextId]?.let {
                        containerStorage.removeContainer(it)
                    }
                }
            }
        }

        next(action)
    }
}
