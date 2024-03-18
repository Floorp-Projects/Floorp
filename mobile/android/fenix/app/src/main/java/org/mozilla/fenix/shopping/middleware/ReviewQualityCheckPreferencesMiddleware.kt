/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.HighlightsCardExpanded
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.InfoCardExpanded
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.SettingsCardExpanded
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.shopping.ShoppingState.CardState
import org.mozilla.fenix.shopping.ShoppingExperienceFeature
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn

/**
 * Middleware for getting and setting review quality check user preferences.
 *
 * @param reviewQualityCheckPreferences The [ReviewQualityCheckPreferences] instance to get and
 * set preferences for the review quality check feature.
 * @param reviewQualityCheckVendorsService The [ReviewQualityCheckVendorsService] instance for
 * getting the list of product vendors.
 * @param appStore The [AppStore] instance for dispatching [ShoppingAction]s.
 * @param shoppingExperienceFeature The [ShoppingExperienceFeature] instance to get feature flags.
 * @param scope The [CoroutineScope] to use for launching coroutines.
 */
class ReviewQualityCheckPreferencesMiddleware(
    private val reviewQualityCheckPreferences: ReviewQualityCheckPreferences,
    private val reviewQualityCheckVendorsService: ReviewQualityCheckVendorsService,
    private val appStore: AppStore,
    private val shoppingExperienceFeature: ShoppingExperienceFeature,
    private val scope: CoroutineScope,
) : ReviewQualityCheckMiddleware {

    override fun invoke(
        context: MiddlewareContext<ReviewQualityCheckState, ReviewQualityCheckAction>,
        next: (ReviewQualityCheckAction) -> Unit,
        action: ReviewQualityCheckAction,
    ) {
        next(action)
        when (action) {
            is ReviewQualityCheckAction.PreferencesMiddlewareAction -> {
                processAction(context.store, action)
            }

            else -> {
                // no-op
            }
        }
    }

    @Suppress("LongMethod")
    private fun processAction(
        store: Store<ReviewQualityCheckState, ReviewQualityCheckAction>,
        action: ReviewQualityCheckAction.PreferencesMiddlewareAction,
    ) {
        when (action) {
            is ReviewQualityCheckAction.Init -> {
                scope.launch {
                    val hasUserOptedIn = reviewQualityCheckPreferences.enabled()
                    val isProductRecommendationsEnabled =
                        reviewQualityCheckPreferences.productRecommendationsEnabled()

                    val updateUserPreferences = if (hasUserOptedIn) {
                        val savedCardState =
                            reviewQualityCheckVendorsService.selectedTabUrl()?.let {
                                appStore.state.shoppingState.productCardState.getOrElse(it) { CardState() }
                            } ?: CardState()

                        ReviewQualityCheckAction.OptInCompleted(
                            isProductRecommendationsEnabled = isProductRecommendationsEnabled,
                            productRecommendationsExposure =
                            shoppingExperienceFeature.isProductRecommendationsExposureEnabled,
                            productVendor = reviewQualityCheckVendorsService.productVendor(),
                            isHighlightsExpanded = savedCardState.isHighlightsExpanded,
                            isInfoExpanded = savedCardState.isInfoExpanded,
                            isSettingsExpanded = savedCardState.isSettingsExpanded,
                        )
                    } else {
                        val productVendors = reviewQualityCheckVendorsService.productVendors()
                        ReviewQualityCheckAction.OptOutCompleted(productVendors)
                    }
                    store.dispatch(updateUserPreferences)
                }
            }

            ReviewQualityCheckAction.OptIn -> {
                scope.launch {
                    val isProductRecommendationsEnabled =
                        reviewQualityCheckPreferences.productRecommendationsEnabled()
                    store.dispatch(
                        ReviewQualityCheckAction.OptInCompleted(
                            isProductRecommendationsEnabled = isProductRecommendationsEnabled,
                            productRecommendationsExposure =
                            shoppingExperienceFeature.isProductRecommendationsExposureEnabled,
                            productVendor = reviewQualityCheckVendorsService.productVendor(),
                            isHighlightsExpanded = false,
                            isInfoExpanded = false,
                            isSettingsExpanded = false,
                        ),
                    )

                    // Update the preference
                    reviewQualityCheckPreferences.setEnabled(true)
                    reviewQualityCheckPreferences.updateCFRCondition(System.currentTimeMillis())
                }
            }

            ReviewQualityCheckAction.OptOut -> {
                scope.launch {
                    // Update the preference
                    reviewQualityCheckPreferences.setEnabled(false)
                }
            }

            ReviewQualityCheckAction.ToggleProductRecommendation -> {
                scope.launch {
                    val productRecommendationsEnabled =
                        reviewQualityCheckPreferences.productRecommendationsEnabled()
                    if (productRecommendationsEnabled != null) {
                        reviewQualityCheckPreferences.setProductRecommendationsEnabled(
                            !productRecommendationsEnabled,
                        )
                    }
                }
            }

            ReviewQualityCheckAction.ExpandCollapseHighlights -> {
                appStore.dispatchShoppingAction(
                    reviewQualityCheckState = store.state,
                    action = { productPageUrl, optedIn ->
                        HighlightsCardExpanded(
                            productPageUrl = productPageUrl,
                            expanded = optedIn.isHighlightsExpanded,
                        )
                    },
                )
            }

            ReviewQualityCheckAction.ExpandCollapseInfo -> {
                appStore.dispatchShoppingAction(
                    reviewQualityCheckState = store.state,
                    action = { productPageUrl, optedIn ->
                        InfoCardExpanded(
                            productPageUrl = productPageUrl,
                            expanded = optedIn.isInfoExpanded,
                        )
                    },
                )
            }

            ReviewQualityCheckAction.ExpandCollapseSettings -> {
                appStore.dispatchShoppingAction(
                    reviewQualityCheckState = store.state,
                    action = { productPageUrl, optedIn ->
                        SettingsCardExpanded(
                            productPageUrl = productPageUrl,
                            expanded = optedIn.isSettingsExpanded,
                        )
                    },
                )
            }
        }
    }

    private fun Store<AppState, AppAction>.dispatchShoppingAction(
        reviewQualityCheckState: ReviewQualityCheckState,
        action: (productPageUrl: String, optedIn: OptedIn) -> ShoppingAction,
    ) {
        if (reviewQualityCheckState is OptedIn) {
            reviewQualityCheckVendorsService.selectedTabUrl()?.let {
                dispatch(action(it, reviewQualityCheckState))
            }
        }
    }
}
