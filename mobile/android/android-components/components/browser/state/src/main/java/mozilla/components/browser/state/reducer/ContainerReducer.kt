/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.ContainerAction
import mozilla.components.browser.state.state.BrowserState

internal object ContainerReducer {
    fun reduce(state: BrowserState, action: ContainerAction): BrowserState {
        return when (action) {
            is ContainerAction.AddContainerAction -> {
                val existingContainer = state.containers[action.container.contextId]
                if (existingContainer == null) {
                    state.copy(
                        containers = state.containers + (action.container.contextId to action.container),
                    )
                } else {
                    state
                }
            }
            is ContainerAction.AddContainersAction -> {
                state.copy(
                    containers = state.containers + (
                        action.containers.map { it.contextId to it }
                            .toMap()
                        ),
                )
            }
            is ContainerAction.RemoveContainerAction -> {
                state.copy(
                    containers = state.containers - action.contextId,
                )
            }
        }
    }
}
