/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.R

class WebsiteInfoStore(
    initialState: WebsiteInfoState
) : Store<WebsiteInfoState, WebsiteInfoAction>(
    initialState, ::websiteInfoReducer
) {
    companion object {
        fun createStore(url: String, isSecured: Boolean): WebsiteInfoStore {
            val (stringRes, iconRes, colorRes) = when (isSecured) {
                true -> getSecuredWebsiteUiValues()
                false -> getInsecureWebsiteUiValues()
            }
            return WebsiteInfoStore(WebsiteInfoState(url, stringRes, iconRes, colorRes))
        }
    }
}

data class WebsiteInfoState(
    val url: String,
    @StringRes val securityInfoRes: Int,
    @DrawableRes val iconRes: Int,
    @ColorRes val iconTintRes: Int
) : State

sealed class WebsiteInfoAction : Action {
    object Stub1 : WebsiteInfoAction()
    object Stub2 : WebsiteInfoAction()
}

fun websiteInfoReducer(
    state: WebsiteInfoState,
    action: WebsiteInfoAction
): WebsiteInfoState {
    return when (action) {
        WebsiteInfoAction.Stub1 -> state
        WebsiteInfoAction.Stub2 -> state
    }
}

private fun getSecuredWebsiteUiValues() = Triple(
    R.string.quick_settings_sheet_secure_connection,
    R.drawable.mozac_ic_lock,
    R.color.photonGreen50
)

private fun getInsecureWebsiteUiValues() = Triple(
    R.string.quick_settings_sheet_insecure_connection,
    R.drawable.mozac_ic_globe,
    R.color.photonRed50
)
