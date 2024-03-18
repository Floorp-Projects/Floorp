/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.annotation.StringRes
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Product analysis error UI
 *
 * @param error The error state to display.
 * @param onReportBackInStockClick Invoked when the user clicks on the report back in stock button.
 * @param productRecommendationsEnabled The current state of the product recommendations toggle.
 * @param productVendor The vendor of the product.
 * @param isSettingsExpanded Whether or not the settings card is expanded.
 * @param isInfoExpanded Whether or not the info card is expanded.
 * @param onReviewGradeLearnMoreClick Invoked when the user clicks to learn more about review grades.
 * @param onOptOutClick Invoked when the user opts out of the review quality check feature.
 * @param onProductRecommendationsEnabledStateChange Invoked when the user changes the product
 * recommendations toggle state.
 * @param onFooterLinkClick Invoked when the user clicks on the footer link.
 * @param onSettingsExpandToggleClick Invoked when the user expands or collapses the settings card.
 * @param onInfoExpandToggleClick Invoked when the user expands or collapses the info card.
 * @param modifier Modifier to apply to the layout.
 */
@Composable
@Suppress("LongParameterList", "LongMethod")
fun ProductAnalysisError(
    error: ProductReviewState.Error,
    onReportBackInStockClick: () -> Unit,
    productRecommendationsEnabled: Boolean?,
    productVendor: ReviewQualityCheckState.ProductVendor,
    isSettingsExpanded: Boolean,
    isInfoExpanded: Boolean,
    onReviewGradeLearnMoreClick: () -> Unit,
    onOptOutClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onFooterLinkClick: () -> Unit,
    onSettingsExpandToggleClick: () -> Unit,
    onInfoExpandToggleClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        val (
            @StringRes titleResourceId: Int,
            @StringRes descriptionResourceId: Int,
            type: ReviewQualityCheckInfoType,
        ) = when (error) {
            ProductReviewState.Error.GenericError -> {
                Triple(
                    R.string.review_quality_check_generic_error_title,
                    R.string.review_quality_check_generic_error_body,
                    ReviewQualityCheckInfoType.Info,
                )
            }

            ProductReviewState.Error.NetworkError -> {
                Triple(
                    R.string.review_quality_check_no_connection_title,
                    R.string.review_quality_check_no_connection_body,
                    ReviewQualityCheckInfoType.Warning,
                )
            }

            ProductReviewState.Error.UnsupportedProductTypeError -> {
                Triple(
                    R.string.review_quality_check_not_analyzable_info_title,
                    R.string.review_quality_check_not_analyzable_info_body,
                    ReviewQualityCheckInfoType.Info,
                )
            }

            ProductReviewState.Error.NotEnoughReviews -> {
                Triple(
                    R.string.review_quality_check_no_reviews_warning_title,
                    R.string.review_quality_check_no_reviews_warning_body,
                    ReviewQualityCheckInfoType.Info,
                )
            }

            ProductReviewState.Error.ProductNotAvailable -> {
                Triple(
                    R.string.review_quality_check_product_availability_warning_title,
                    R.string.review_quality_check_product_availability_warning_body,
                    ReviewQualityCheckInfoType.Info,
                )
            }

            ProductReviewState.Error.ProductAlreadyReported -> {
                Triple(
                    R.string.review_quality_check_analysis_requested_other_user_info_title,
                    R.string.review_quality_check_analysis_requested_other_user_info_body,
                    ReviewQualityCheckInfoType.Info,
                )
            }

            ProductReviewState.Error.ThanksForReporting -> {
                Triple(
                    R.string.review_quality_check_analysis_requested_info_title,
                    R.string.review_quality_check_analysis_requested_info_body,
                    ReviewQualityCheckInfoType.Info,
                )
            }
        }

        if (error == ProductReviewState.Error.ProductNotAvailable) {
            ReviewQualityCheckInfoCard(
                title = stringResource(id = titleResourceId),
                description = stringResource(id = descriptionResourceId),
                type = type,
                modifier = Modifier.fillMaxWidth(),
                buttonText = InfoCardButtonText(
                    text = stringResource(R.string.review_quality_check_product_availability_warning_action_2),
                    onClick = onReportBackInStockClick,
                ),
            )
        } else {
            ReviewQualityCheckInfoCard(
                title = stringResource(id = titleResourceId),
                description = stringResource(id = descriptionResourceId),
                type = type,
                modifier = Modifier.fillMaxWidth(),
            )
        }

        ReviewQualityInfoCard(
            productVendor = productVendor,
            isExpanded = isInfoExpanded,
            onLearnMoreClick = onReviewGradeLearnMoreClick,
            onExpandToggleClick = onInfoExpandToggleClick,
        )

        ReviewQualityCheckSettingsCard(
            productRecommendationsEnabled = productRecommendationsEnabled,
            isExpanded = isSettingsExpanded,
            onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
            onTurnOffReviewQualityCheckClick = onOptOutClick,
            onExpandToggleClick = onSettingsExpandToggleClick,
            modifier = Modifier.fillMaxWidth(),
        )

        ReviewQualityCheckFooter(
            onLinkClick = onFooterLinkClick,
        )
    }
}

@Composable
@LightDarkPreview
private fun ProductAnalysisErrorPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(color = FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            var productRecommendationsEnabled by remember { mutableStateOf(false) }
            var isSettingsExpanded by remember { mutableStateOf(false) }
            var isInfoExpanded by remember { mutableStateOf(false) }

            ProductAnalysisError(
                error = ProductReviewState.Error.NetworkError,
                onReportBackInStockClick = { },
                productRecommendationsEnabled = productRecommendationsEnabled,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                isSettingsExpanded = isSettingsExpanded,
                isInfoExpanded = isInfoExpanded,
                onReviewGradeLearnMoreClick = {},
                onOptOutClick = {},
                onProductRecommendationsEnabledStateChange = { productRecommendationsEnabled = it },
                onFooterLinkClick = {},
                onSettingsExpandToggleClick = { isSettingsExpanded = !isSettingsExpanded },
                onInfoExpandToggleClick = { isInfoExpanded = !isInfoExpanded },
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
