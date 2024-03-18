/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions.permissionoptions

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.focus.settings.permissions.SitePermissionOption

class SitePermissionOptionsScreenStore(
    initialState: SitePermissionOptionsScreenState,
    middlewares: List<Middleware<SitePermissionOptionsScreenState, SitePermissionOptionsScreenAction>> = emptyList(),
) : Store<SitePermissionOptionsScreenState, SitePermissionOptionsScreenAction>(
    initialState,
    ::sitePermissionOptionsScreenReducer,
    middlewares,
) {
    init {
        dispatch(SitePermissionOptionsScreenAction.InitSitePermissionOptions)
    }
}

data class SitePermissionOptionsScreenState(
    val sitePermissionOptionList: List<SitePermissionOption> = emptyList(),
    val selectedSitePermissionOption: SitePermissionOption? = null,
    val sitePermissionLabel: String = "",
    val isAndroidPermissionGranted: Boolean = false,
) : State

sealed class SitePermissionOptionsScreenAction : Action {
    object InitSitePermissionOptions : SitePermissionOptionsScreenAction()
    data class Select(val selectedSitePermissionOption: SitePermissionOption) : SitePermissionOptionsScreenAction()
    data class AndroidPermission(val isAndroidPermissionGranted: Boolean) : SitePermissionOptionsScreenAction()
    data class UpdateSitePermissionOptions(
        val sitePermissionOptionsList: List<SitePermissionOption>,
        val selectedSitePermissionOption: SitePermissionOption,
        val sitePermissionLabel: String,
        val isAndroidPermissionGranted: Boolean,
    ) : SitePermissionOptionsScreenAction()
}

private fun sitePermissionOptionsScreenReducer(
    state: SitePermissionOptionsScreenState,
    action: SitePermissionOptionsScreenAction,
): SitePermissionOptionsScreenState {
    return when (action) {
        is SitePermissionOptionsScreenAction.Select -> {
            state.copy(selectedSitePermissionOption = action.selectedSitePermissionOption)
        }
        is SitePermissionOptionsScreenAction.UpdateSitePermissionOptions -> {
            state.copy(
                sitePermissionOptionList = action.sitePermissionOptionsList,
                selectedSitePermissionOption = action.selectedSitePermissionOption,
                sitePermissionLabel = action.sitePermissionLabel,
                isAndroidPermissionGranted = action.isAndroidPermissionGranted,
            )
        }

        SitePermissionOptionsScreenAction.InitSitePermissionOptions -> {
            throw IllegalStateException(
                "You need to add SitePermissionsOptionsMiddleware to your SitePermissionsOptionsScreenStore. ($action)",
            )
        }
        is SitePermissionOptionsScreenAction.AndroidPermission -> {
            state.copy(isAndroidPermissionGranted = action.isAndroidPermissionGranted)
        }
    }
}
