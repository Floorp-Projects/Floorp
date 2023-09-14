/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.animation.Crossfade
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.spring
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.HighlightType
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent
import org.mozilla.fenix.shopping.store.forCompactMode
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * UI for review quality check content displaying product analysis.
 *
 * @param productRecommendationsEnabled The current state of the product recommendations toggle.
 * @param productAnalysis The product analysis to display.
 * @param onOptOutClick Invoked when the user opts out of the review quality check feature.
 * @param onProductRecommendationsEnabledStateChange Invoked when the user changes the product
 * recommendations toggle state.
 * @param onReviewGradeLearnMoreClick Invoked when the user clicks to learn more about review grades.
 * @param onBylineLinkClick Invoked when the user clicks on the byline link.
 * @param modifier The modifier to be applied to the Composable.
 */
@Composable
@Suppress("LongParameterList")
fun ProductAnalysis(
    productRecommendationsEnabled: Boolean,
    productAnalysis: AnalysisPresent,
    onOptOutClick: () -> Unit,
    onReanalyzeClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onReviewGradeLearnMoreClick: (String) -> Unit,
    onBylineLinkClick: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        if (productAnalysis.needsAnalysis) {
            ReanalyzeCard(
                onReanalyzeClick = onReanalyzeClick,
            )
        }

        if (productAnalysis.reviewGrade != null) {
            ReviewGradeCard(
                reviewGrade = productAnalysis.reviewGrade,
                modifier = Modifier.fillMaxWidth(),
            )
        }

        if (productAnalysis.adjustedRating != null) {
            AdjustedProductRatingCard(
                rating = productAnalysis.adjustedRating,
                modifier = Modifier.fillMaxWidth(),
            )
        }

        if (productAnalysis.highlights != null) {
            HighlightsCard(
                highlights = productAnalysis.highlights,
                highlightsFadeVisible = productAnalysis.highlightsFadeVisible,
                showMoreButtonVisible = productAnalysis.showMoreButtonVisible,
                modifier = Modifier.fillMaxWidth(),
            )
        }

        ReviewQualityInfoCard(
            modifier = Modifier.fillMaxWidth(),
            onLearnMoreClick = onReviewGradeLearnMoreClick,
        )

        ReviewQualityCheckSettingsCard(
            productRecommendationsEnabled = productRecommendationsEnabled,
            onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
            onTurnOffReviewQualityCheckClick = onOptOutClick,
            modifier = Modifier.fillMaxWidth(),
        )

        ReviewQualityCheckFooter(
            onLinkClick = onBylineLinkClick,
        )
    }
}

@Composable
private fun ReanalyzeCard(
    onReanalyzeClick: () -> Unit,
) {
    ReviewQualityCheckInfoCard(
        title = stringResource(R.string.review_quality_check_outdated_analysis_warning_title),
        type = ReviewQualityCheckInfoType.AnalysisUpdate,
        modifier = Modifier.fillMaxWidth(),
        buttonText = stringResource(R.string.review_quality_check_outdated_analysis_warning_action),
        onButtonClick = onReanalyzeClick,
        icon = {
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_information_fill_24),
                contentDescription = null,
                modifier = Modifier.size(24.dp),
                tint = FirefoxTheme.colors.iconPrimary,
            )
        },
    )
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

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun AdjustedProductRatingCard(
    rating: Float,
    modifier: Modifier = Modifier,
) {
    ReviewQualityCheckCard(modifier = modifier.semantics(mergeDescendants = true) {}) {
        FlowRow(
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(
                text = stringResource(R.string.review_quality_check_adjusted_rating_title),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline8,
                modifier = Modifier.padding(
                    end = 16.dp,
                    bottom = 8.dp,
                ),
            )

            StarRating(value = rating)
        }

        Spacer(modifier = Modifier.height(8.dp))

        Text(
            text = stringResource(R.string.review_quality_check_adjusted_rating_description),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.caption,
        )
    }
}

@Composable
private fun HighlightsCard(
    highlights: Map<HighlightType, List<String>>,
    highlightsFadeVisible: Boolean,
    showMoreButtonVisible: Boolean,
    modifier: Modifier = Modifier,
) {
    ReviewQualityCheckCard(modifier = modifier) {
        var isExpanded by remember { mutableStateOf(false) }
        val highlightsForCompactMode = remember(highlights) { highlights.forCompactMode() }
        val highlightsToDisplay = remember(isExpanded, highlights) {
            if (isExpanded) {
                highlights
            } else {
                highlightsForCompactMode
            }
        }

        Text(
            text = stringResource(R.string.review_quality_check_highlights_title),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline8,
        )

        Spacer(modifier = Modifier.height(16.dp))

        Box(
            contentAlignment = Alignment.BottomCenter,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .animateContentSize(animationSpec = spring()),
            ) {
                highlightsToDisplay.forEach { highlight ->
                    HighlightTitle(highlight.key)

                    Spacer(modifier = Modifier.height(8.dp))

                    highlight.value.forEach {
                        HighlightText(it)

                        Spacer(modifier = Modifier.height(4.dp))
                    }

                    if (highlightsToDisplay.entries.last().key != highlight.key) {
                        Spacer(modifier = Modifier.height(16.dp))
                    }
                }
            }

            Crossfade(
                targetState = isExpanded,
                label = "HighlightsCard-Crossfade",
            ) { expanded ->
                if (expanded.not() && highlightsFadeVisible) {
                    Spacer(
                        modifier = Modifier
                            .height(32.dp)
                            .fillMaxWidth()
                            .background(
                                brush = Brush.verticalGradient(
                                    colors = listOf(
                                        FirefoxTheme.colors.layer2.copy(alpha = 0f),
                                        FirefoxTheme.colors.layer2,
                                    ),
                                ),
                            ),
                    )
                }
            }
        }

        if (showMoreButtonVisible) {
            Spacer(modifier = Modifier.height(8.dp))

            SecondaryButton(
                text = if (isExpanded) {
                    stringResource(R.string.review_quality_check_highlights_show_less)
                } else {
                    stringResource(R.string.review_quality_check_highlights_show_more)
                },
                onClick = { isExpanded = isExpanded.not() },
            )
        }
    }
}

@Composable
private fun HighlightText(text: String) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Spacer(modifier = Modifier.width(32.dp))

        Text(
            text = text,
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )
    }
}

@Composable
private fun HighlightTitle(highlightType: HighlightType) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val highlight = remember(highlightType) { highlightType.toHighlight() }

        Icon(
            painter = painterResource(id = highlight.iconResourceId),
            tint = FirefoxTheme.colors.iconPrimary,
            contentDescription = null,
        )

        Spacer(modifier = Modifier.width(8.dp))

        Text(
            text = stringResource(id = highlight.titleResourceId),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline8,
        )
    }
}

private fun HighlightType.toHighlight() =
    when (this) {
        HighlightType.QUALITY -> Highlight.QUALITY
        HighlightType.PRICE -> Highlight.PRICE
        HighlightType.SHIPPING -> Highlight.SHIPPING
        HighlightType.PACKAGING_AND_APPEARANCE -> Highlight.PACKAGING_AND_APPEARANCE
        HighlightType.COMPETITIVENESS -> Highlight.COMPETITIVENESS
    }

private enum class Highlight(
    val titleResourceId: Int,
    val iconResourceId: Int,
) {
    QUALITY(
        titleResourceId = R.string.review_quality_check_highlights_type_quality,
        iconResourceId = R.drawable.mozac_ic_quality_24,
    ),
    PRICE(
        titleResourceId = R.string.review_quality_check_highlights_type_price,
        iconResourceId = R.drawable.mozac_ic_price_24,
    ),
    SHIPPING(
        titleResourceId = R.string.review_quality_check_highlights_type_shipping,
        iconResourceId = R.drawable.mozac_ic_shipping_24,
    ),
    PACKAGING_AND_APPEARANCE(
        titleResourceId = R.string.review_quality_check_highlights_type_packaging_appearance,
        iconResourceId = R.drawable.mozac_ic_packaging_24,
    ),
    COMPETITIVENESS(
        titleResourceId = R.string.review_quality_check_highlights_type_competitiveness,
        iconResourceId = R.drawable.mozac_ic_competitiveness_24,
    ),
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
                    highlights = sortedMapOf(
                        HighlightType.QUALITY to listOf(
                            "High quality",
                            "Excellent craftsmanship",
                            "Superior materials",
                        ),
                        HighlightType.PRICE to listOf(
                            "Affordable prices",
                            "Great value for money",
                            "Discounted offers",
                        ),
                        HighlightType.SHIPPING to listOf(
                            "Fast and reliable shipping",
                            "Free shipping options",
                            "Express delivery",
                        ),
                        HighlightType.PACKAGING_AND_APPEARANCE to listOf(
                            "Elegant packaging",
                            "Attractive appearance",
                            "Beautiful design",
                        ),
                        HighlightType.COMPETITIVENESS to listOf(
                            "Competitive pricing",
                            "Strong market presence",
                            "Unbeatable deals",
                        ),
                    ),
                ),
                onOptOutClick = {},
                onReanalyzeClick = {},
                onProductRecommendationsEnabledStateChange = {
                    productRecommendationsEnabled.value = it
                },
                onReviewGradeLearnMoreClick = {},
                onBylineLinkClick = {},
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun ReanalyzeCardPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(color = FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            ReanalyzeCard(
                onReanalyzeClick = {},
            )
        }
    }
}
