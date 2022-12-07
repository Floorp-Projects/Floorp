/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerexception

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

class CookieBannerExceptionStore(
    initialState: CookieBannerExceptionState,
    middlewares: List<Middleware<CookieBannerExceptionState, CookieBannerExceptionAction>> = emptyList(),
) : Store<CookieBannerExceptionState, CookieBannerExceptionAction>(
    initialState,
    ::cookieBannerExceptionStateReducer,
    middlewares,
) {
    init {
        dispatch(CookieBannerExceptionAction.InitCookieBannerException)
    }
}

/**
 * The state of the language selection
 *
 * @property isCookieBannerToggleEnabled Current status of cookie banner toggle from details exception
 * @property hasException The current status of cookie banner exception from a site
 * @property shouldShowCookieBannerItem Visibility of cookie banner exception item
 */
data class CookieBannerExceptionState(
    val isCookieBannerToggleEnabled: Boolean = false,
    val hasException: Boolean = false,
    val shouldShowCookieBannerItem: Boolean = false,
) : State

/**
 * Action to dispatch through the `CookieBannerExceptionStore` to modify cookie banner exception item and item details
 * from Tracking protection panel through the reducer.
 */
sealed class CookieBannerExceptionAction : Action {
    object InitCookieBannerException : CookieBannerExceptionAction()

    data class ToggleCookieBannerExceptionException(
        val isCookieBannerHandlingExceptionEnabled: Boolean,
    ) : CookieBannerExceptionAction()

    data class UpdateCookieBannerExceptionExceptionVisibility(
        val shouldShowCookieBannerItem: Boolean,
    ) : CookieBannerExceptionAction()

    data class UpdateCookieBannerExceptionException(val hasException: Boolean) : CookieBannerExceptionAction()
}

/**
 * Reduces the cookie banner exception state from the current state and an action performed on it.
 *
 * @param state the current cookie banner item state
 * @param action the action to perform
 * @return the new cookie banner exception state
 */
private fun cookieBannerExceptionStateReducer(
    state: CookieBannerExceptionState,
    action: CookieBannerExceptionAction,
): CookieBannerExceptionState {
    return when (action) {
        is CookieBannerExceptionAction.ToggleCookieBannerExceptionException -> {
            state.copy(isCookieBannerToggleEnabled = action.isCookieBannerHandlingExceptionEnabled)
        }
        is CookieBannerExceptionAction.UpdateCookieBannerExceptionExceptionVisibility -> {
            state.copy(shouldShowCookieBannerItem = action.shouldShowCookieBannerItem)
        }
        is CookieBannerExceptionAction.UpdateCookieBannerExceptionException -> {
            state.copy(hasException = action.hasException)
        }
        CookieBannerExceptionAction.InitCookieBannerException -> {
            throw IllegalStateException(
                "You need to add CookieBannerExceptionMiddleware to your CookieBannerStore. ($action)",
            )
        }
    }
}
