/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.state

import mozilla.components.lib.state.Action

/**
 * Actions for review quality check feature.
 */
sealed interface ReviewQualityCheckAction : Action {

    /**
     * Actions that are observed by middlewares.
     */
    sealed interface MiddlewareAction : ReviewQualityCheckAction

    /**
     * Actions that cause updates to state.
     */
    sealed interface UpdateAction : ReviewQualityCheckAction

    /**
     * Actions related to preferences.
     */
    sealed interface PreferencesMiddlewareAction : MiddlewareAction

    /**
     * Triggered when the store is initialized.
     */
    object Init : PreferencesMiddlewareAction

    /**
     * Triggered when the user has opted in to the review quality check feature.
     */
    object OptIn : PreferencesMiddlewareAction

    /**
     * Triggered when the user has opted out of the review quality check feature.
     */
    object OptOut : PreferencesMiddlewareAction, UpdateAction

    /**
     * Triggered when the user has enabled or disabled product recommendations.
     */
    object ToggleProductRecommendation : PreferencesMiddlewareAction, UpdateAction

    /**
     * Triggered as a result of a [PreferencesMiddlewareAction] to update the state.
     */
    data class UpdateUserPreferences(
        val hasUserOptedIn: Boolean,
        val isProductRecommendationsEnabled: Boolean,
    ) : UpdateAction
}
