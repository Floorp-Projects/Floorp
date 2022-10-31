/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.store

internal object CustomTabsServiceStateReducer {

    fun reduce(state: CustomTabsServiceState, action: CustomTabsAction): CustomTabsServiceState {
        val tabState = state.tabs.getOrElse(action.token) { CustomTabState() }
        val newTabState = reduceTab(tabState, action)
        return state.copy(tabs = state.tabs + Pair(action.token, newTabState))
    }

    private fun reduceTab(state: CustomTabState, action: CustomTabsAction): CustomTabState {
        return when (action) {
            is SaveCreatorPackageNameAction ->
                state.copy(creatorPackageName = action.packageName)
            is ValidateRelationshipAction ->
                state.copy(
                    relationships = state.relationships + Pair(
                        OriginRelationPair(action.origin, action.relation),
                        action.status,
                    ),
                )
        }
    }
}
