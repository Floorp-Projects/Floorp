/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerreducer

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [CookieBannerReducerStore] holds the [CookieBannerReducerState] (state tree).
 *
 * The only way to change the [CookieBannerReducerState] inside
 * [CookieBannerReducerStore] is to dispatch an [CookieBannerReducerAction] on it.
 */
class CookieBannerReducerStore(
    initialState: CookieBannerReducerState,
    middlewares: List<Middleware<CookieBannerReducerState, CookieBannerReducerAction>> = emptyList(),
) : Store<CookieBannerReducerState, CookieBannerReducerAction>(
    initialState,
    ::cookieBannerStateReducer,
    middlewares,
) {
    init {
        dispatch(CookieBannerReducerAction.InitCookieBannerReducer)
    }
}

/**
 * The state of the cookie banner reducer
 *
 * @property isCookieBannerToggleEnabled Current status of cookie banner toggle from details exception.
 * @property shouldShowCookieBannerItem Visibility of cookie banner reducer item.
 * @property isCookieBannerDetected If the site has a cookie banner.
 * @property showSnackBarForSiteToReport When cookie banner reducer doesn't work
 * on a website and the user reports that site
 * @property cookieBannerReducerStatus Current status of cookie banner reducer.
 * @property siteToReport Site to report when cookie banner reducer doesn't work
 */
data class CookieBannerReducerState(
    val isCookieBannerToggleEnabled: Boolean = false,
    val shouldShowCookieBannerItem: Boolean = false,
    val isCookieBannerDetected: Boolean = false,
    val showSnackBarForSiteToReport: Boolean = false,
    val cookieBannerReducerStatus: CookieBannerReducerStatus? = CookieBannerReducerStatus.NoException,
    val siteToReport: String = "",
) : State

/**
 * Action to dispatch through the `CookieBannerReducerStore` to modify cookie banner reducer item and item details
 * from Tracking protection panel through the reducer.
 */
@Suppress("UndocumentedPublicClass")
sealed class CookieBannerReducerAction : Action {
    object InitCookieBannerReducer : CookieBannerReducerAction()

    data class ToggleCookieBannerException(
        val isCookieBannerHandlingExceptionEnabled: Boolean,
    ) : CookieBannerReducerAction()

    data class UpdateCookieBannerReducerVisibility(
        val shouldShowCookieBannerItem: Boolean,
    ) : CookieBannerReducerAction()

    data class UpdateCookieBannerReducerStatus(
        val cookieBannerReducerStatus: CookieBannerReducerStatus?,
    ) : CookieBannerReducerAction()

    data class RequestReportSite(
        val siteToReport: String,
    ) : CookieBannerReducerAction()

    data class ShowSnackBarForSiteToReport(
        val isSnackBarVisible: Boolean,
    ) : CookieBannerReducerAction()
}

/**
 * Reduces the cookie banner state from the current state and an action performed on it.
 *
 * @param state the current cookie banner item state
 * @param action the action to perform
 * @return the new cookie banner reducer state
 */
private fun cookieBannerStateReducer(
    state: CookieBannerReducerState,
    action: CookieBannerReducerAction,
): CookieBannerReducerState {
    return when (action) {
        is CookieBannerReducerAction.ToggleCookieBannerException -> {
            state.copy(isCookieBannerToggleEnabled = action.isCookieBannerHandlingExceptionEnabled)
        }
        is CookieBannerReducerAction.UpdateCookieBannerReducerVisibility -> {
            state.copy(shouldShowCookieBannerItem = action.shouldShowCookieBannerItem)
        }
        is CookieBannerReducerAction.UpdateCookieBannerReducerStatus -> {
            state.copy(cookieBannerReducerStatus = action.cookieBannerReducerStatus)
        }
        is CookieBannerReducerAction.RequestReportSite -> {
            state.copy(siteToReport = action.siteToReport)
        }
        is CookieBannerReducerAction.ShowSnackBarForSiteToReport -> {
            state.copy(showSnackBarForSiteToReport = action.isSnackBarVisible)
        }
        CookieBannerReducerAction.InitCookieBannerReducer -> {
            throw IllegalStateException(
                "You need to add CookieBannerReducerMiddleware to your CookieBannerReducerStore. ($action)",
            )
        }
    }
}
