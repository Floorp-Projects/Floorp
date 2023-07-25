/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.shopping.state.ReviewQualityCheckState
import org.mozilla.fenix.shopping.state.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * UI for review quality check content displaying product analysis.
 *
 * @param productRecommendationsEnabled The current state of the product recommendations toggle.
 * @param productAnalysis The product analysis to display.
 * @param onOptOutClick Invoked when the user opts out of the review quality check feature.
 * @param onProductRecommendationsEnabledStateChange Invoked when the user changes the product
 * recommendations toggle state.
 * @param modifier The modifier to be applied to the Composable.
 */
@Composable
fun ProductAnalysis(
    productRecommendationsEnabled: Boolean,
    productAnalysis: AnalysisPresent,
    onOptOutClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        ReviewGradeCard(
            reviewGrade = productAnalysis.reviewGrade,
            modifier = Modifier.fillMaxWidth(),
        )

        SettingsCard(
            modifier = Modifier.fillMaxWidth(),
            productRecommendationsEnabled = productRecommendationsEnabled,
            onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
            onTurnOffReviewQualityCheckClick = onOptOutClick,
        )
    }
}

@Composable
private fun ReviewGradeCard(
    reviewGrade: ReviewQualityCheckState.Grade,
    modifier: Modifier = Modifier,
) {
    ReviewQualityCheckCard(modifier = modifier.semantics(mergeDescendants = true) {}) {
        Text(
            text = stringResource(R.string.review_quality_check_grade_title),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline8,
        )

        Spacer(modifier = Modifier.height(8.dp))

        ReviewGradeExpanded(grade = reviewGrade)
    }
}

@Composable
private fun SettingsCard(
    productRecommendationsEnabled: Boolean,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onTurnOffReviewQualityCheckClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    ReviewQualityCheckExpandableCard(
        modifier = modifier,
        title = stringResource(R.string.review_quality_check_settings_title),
    ) {
        Column {
            Spacer(modifier = Modifier.height(8.dp))

            SwitchWithLabel(
                checked = productRecommendationsEnabled,
                onCheckedChange = onProductRecommendationsEnabledStateChange,
                label = stringResource(R.string.review_quality_check_settings_recommended_products),
            )

            Spacer(modifier = Modifier.height(16.dp))

            SecondaryButton(
                text = stringResource(R.string.review_quality_check_settings_turn_off),
                onClick = onTurnOffReviewQualityCheckClick,
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun ProductAnalysisPreview() {
    FirefoxTheme {
        ReviewQualityCheckScaffold(
            onRequestDismiss = {},
        ) {
            val productRecommendationsEnabled = remember { mutableStateOf(false) }

            ProductAnalysis(
                productRecommendationsEnabled = productRecommendationsEnabled.value,
                productAnalysis = AnalysisPresent(
                    productId = "123",
                    reviewGrade = ReviewQualityCheckState.Grade.B,
                    needsAnalysis = false,
                    adjustedRating = 3.6f,
                    productUrl = "123",
                    highlights = mapOf(
                        ReviewQualityCheckState.HighlightType.QUALITY to listOf(
                            "High quality",
                            "Excellent craftsmanship",
                            "Superior materials",
                        ),
                        ReviewQualityCheckState.HighlightType.PRICE to listOf(
                            "Affordable prices",
                            "Great value for money",
                            "Discounted offers",
                        ),
                        ReviewQualityCheckState.HighlightType.SHIPPING to listOf(
                            "Fast and reliable shipping",
                            "Free shipping options",
                            "Express delivery",
                        ),
                        ReviewQualityCheckState.HighlightType.PACKAGING_AND_APPEARANCE to listOf(
                            "Elegant packaging",
                            "Attractive appearance",
                            "Beautiful design",
                        ),
                        ReviewQualityCheckState.HighlightType.COMPETITIVENESS to listOf(
                            "Competitive pricing",
                            "Strong market presence",
                            "Unbeatable deals",
                        ),
                    ),
                ),
                onOptOutClick = {},
                onProductRecommendationsEnabledStateChange = {
                    productRecommendationsEnabled.value = it
                },
            )
        }
    }
}
