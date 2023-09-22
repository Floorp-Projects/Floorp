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
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Product analysis error UI
 *
 * @param error The error state to display.
 * @param productRecommendationsEnabled The current state of the product recommendations toggle.
 * @param onReviewGradeLearnMoreClick Invoked when the user clicks to learn more about review grades.
 * @param onOptOutClick Invoked when the user opts out of the review quality check feature.
 * @param onProductRecommendationsEnabledStateChange Invoked when the user changes the product
 * recommendations toggle state.
 * @param onFooterLinkClick Invoked when the user clicks on the footer link.
 * @param modifier Modifier to apply to the layout.
 */
@Composable
@Suppress("LongParameterList")
fun ProductAnalysisError(
    error: ProductReviewState.Error,
    productRecommendationsEnabled: Boolean?,
    onReviewGradeLearnMoreClick: (String) -> Unit,
    onOptOutClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onFooterLinkClick: (String) -> Unit,
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
        }

        ReviewQualityCheckInfoCard(
            title = stringResource(id = titleResourceId),
            description = stringResource(id = descriptionResourceId),
            type = type,
            modifier = Modifier.fillMaxWidth(),
        )

        ReviewQualityInfoCard(
            onLearnMoreClick = onReviewGradeLearnMoreClick,
        )

        ReviewQualityCheckSettingsCard(
            productRecommendationsEnabled = productRecommendationsEnabled,
            onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
            onTurnOffReviewQualityCheckClick = onOptOutClick,
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
            ProductAnalysisError(
                error = ProductReviewState.Error.NetworkError,
                productRecommendationsEnabled = true,
                onReviewGradeLearnMoreClick = {},
                onOptOutClick = {},
                onProductRecommendationsEnabledStateChange = {},
                onFooterLinkClick = {},
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
