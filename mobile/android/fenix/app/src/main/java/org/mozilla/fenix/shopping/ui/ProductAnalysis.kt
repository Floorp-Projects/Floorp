/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.animation.Crossfade
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.spring
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.shape.RoundedCornerShape
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
import androidx.compose.ui.layout.layout
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.Image
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.compose.ext.onShown
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.HighlightType
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.HighlightsInfo
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.RecommendedProductState
import org.mozilla.fenix.shopping.ui.ext.headingResource
import org.mozilla.fenix.theme.FirefoxTheme

private val combinedParentHorizontalPadding = 32.dp
private val productRecommendationImageSize = 60.dp

private const val PRODUCT_RECOMMENDATION_SETTLE_TIME_MS = 1500
private const val PRODUCT_RECOMMENDATION_IMPRESSION_THRESHOLD = 0.5f

/**
 * UI for review quality check content displaying product analysis.
 *
 * @param productRecommendationsEnabled The current state of the product recommendations toggle.
 * @param productAnalysis The product analysis to display.
 * @param productVendor The vendor of the product.
 * @param isSettingsExpanded Whether or not the settings card is expanded.
 * @param isInfoExpanded Whether or not the info card is expanded.
 * @param isHighlightsExpanded Whether or not the highlights card is expanded.
 * @param onOptOutClick Invoked when the user opts out of the review quality check feature.
 * @param onReanalyzeClick Invoked when the user clicks to re-analyze a product.
 * @param onProductRecommendationsEnabledStateChange Invoked when the user changes the product
 * recommendations toggle state.
 * @param onReviewGradeLearnMoreClick Invoked when the user clicks to learn more about review grades.
 * @param onFooterLinkClick Invoked when the user clicks on the footer link.
 * @param onHighlightsExpandToggleClick Invoked when the user clicks to show more recent reviews.
 * @param onSettingsExpandToggleClick Invoked when the user expands or collapses the settings card.
 * @param onInfoExpandToggleClick Invoked when the user expands or collapses the info card.
 * @param onRecommendedProductClick Invoked when the user clicks on the product recommendation.
 * @param onRecommendedProductImpression Invoked when the user has seen the product recommendation.
 * @param modifier The modifier to be applied to the Composable.
 */
@Composable
@Suppress("LongParameterList")
fun ProductAnalysis(
    productRecommendationsEnabled: Boolean?,
    productAnalysis: AnalysisPresent,
    productVendor: ReviewQualityCheckState.ProductVendor,
    isSettingsExpanded: Boolean,
    isInfoExpanded: Boolean,
    isHighlightsExpanded: Boolean,
    onOptOutClick: () -> Unit,
    onReanalyzeClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onReviewGradeLearnMoreClick: () -> Unit,
    onFooterLinkClick: () -> Unit,
    onHighlightsExpandToggleClick: () -> Unit,
    onSettingsExpandToggleClick: () -> Unit,
    onInfoExpandToggleClick: () -> Unit,
    onRecommendedProductClick: (aid: String, url: String) -> Unit,
    onRecommendedProductImpression: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        when (val analysisStatus = productAnalysis.analysisStatus) {
            is AnalysisStatus.NeedsAnalysis -> {
                ReanalyzeCard(onReanalyzeClick = onReanalyzeClick)
            }

            is AnalysisStatus.Reanalyzing -> {
                ReanalysisInProgress(analysisStatus)
            }

            is AnalysisStatus.UpToDate -> {
                // no-op
            }
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

        if (productAnalysis.highlightsInfo != null) {
            HighlightsCard(
                highlightsInfo = productAnalysis.highlightsInfo,
                onHighlightsExpandToggleClick = onHighlightsExpandToggleClick,
                isExpanded = isHighlightsExpanded,
                modifier = Modifier.fillMaxWidth(),
            )
        }

        ReviewQualityInfoCard(
            productVendor = productVendor,
            isExpanded = isInfoExpanded,
            modifier = Modifier.fillMaxWidth(),
            onExpandToggleClick = onInfoExpandToggleClick,
            onLearnMoreClick = onReviewGradeLearnMoreClick,
        )

        if (productAnalysis.recommendedProductState is RecommendedProductState.Product) {
            ProductRecommendation(
                product = productAnalysis.recommendedProductState,
                onClick = onRecommendedProductClick,
                onImpression = onRecommendedProductImpression,
            )
        }

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
private fun ReanalyzeCard(
    onReanalyzeClick: () -> Unit,
) {
    ReviewQualityCheckInfoCard(
        title = stringResource(R.string.review_quality_check_outdated_analysis_warning_title),
        type = ReviewQualityCheckInfoType.AnalysisUpdate,
        modifier = Modifier.fillMaxWidth(),
        buttonText = InfoCardButtonText(
            text = stringResource(R.string.review_quality_check_outdated_analysis_warning_action),
            onClick = onReanalyzeClick,
        ),
    )
}

@Composable
private fun ReanalysisInProgress(reanalyzing: AnalysisStatus.Reanalyzing) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        modifier = Modifier.padding(vertical = 8.dp),
    ) {
        DeterminateProgressIndicator(
            progress = reanalyzing.progress.normalizedProgress,
            modifier = Modifier.size(24.dp),
        )

        Text(
            text = stringResource(
                id = R.string.review_quality_check_analysis_in_progress_warning_title_2,
                reanalyzing.progress.formattedProgress,
            ),
            style = FirefoxTheme.typography.subtitle1,
            color = FirefoxTheme.colors.textPrimary,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

@Composable
private fun ReviewGradeCard(
    reviewGrade: ReviewQualityCheckState.Grade,
    modifier: Modifier = Modifier,
) {
    ReviewQualityCheckCard(modifier = modifier.semantics(mergeDescendants = true) { heading() }) {
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
    ReviewQualityCheckCard(modifier = modifier.semantics(mergeDescendants = true) { heading() }) {
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

            StarRating(
                value = rating,
                modifier = Modifier.padding(bottom = 8.dp),
            )
        }

        Text(
            text = stringResource(R.string.review_quality_check_adjusted_rating_description_2),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.caption,
        )
    }
}

@Suppress("LongMethod")
@Composable
private fun HighlightsCard(
    highlightsInfo: HighlightsInfo,
    isExpanded: Boolean,
    onHighlightsExpandToggleClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    ReviewQualityCheckCard(modifier = modifier) {
        val highlightsToDisplay = remember(isExpanded, highlightsInfo.highlights) {
            if (isExpanded) {
                highlightsInfo.highlights
            } else {
                highlightsInfo.highlightsForCompactMode
            }
        }
        val titleContentDescription =
            headingResource(id = R.string.review_quality_check_highlights_title)

        Text(
            text = stringResource(R.string.review_quality_check_highlights_title),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline8,
            modifier = Modifier.semantics {
                heading()
                contentDescription = titleContentDescription
            },
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
                if (expanded.not() && highlightsInfo.highlightsFadeVisible) {
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

        if (highlightsInfo.showMoreButtonVisible) {
            Spacer(modifier = Modifier.height(8.dp))

            Divider(modifier = Modifier.extendWidthToParentBorder())

            Spacer(modifier = Modifier.height(8.dp))

            SecondaryButton(
                text = if (isExpanded) {
                    stringResource(R.string.review_quality_check_highlights_show_less)
                } else {
                    stringResource(R.string.review_quality_check_highlights_show_more)
                },
                onClick = onHighlightsExpandToggleClick,
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
            text = stringResource(id = R.string.surrounded_with_quotes, text),
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

private fun Modifier.extendWidthToParentBorder(): Modifier =
    this.layout { measurable, constraints ->
        val placeable = measurable.measure(
            constraints.copy(
                maxWidth = constraints.maxWidth + combinedParentHorizontalPadding.roundToPx(),
            ),
        )
        layout(placeable.width, placeable.height) {
            placeable.place(0, 0)
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
private fun ProductRecommendation(
    product: RecommendedProductState.Product,
    onClick: (String, String) -> Unit,
    onImpression: (String) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        ReviewQualityCheckCard(
            modifier = Modifier
                .fillMaxWidth()
                .semantics { heading() }
                .clickable {
                    onClick(product.aid, product.productUrl)
                }
                .onShown(
                    threshold = PRODUCT_RECOMMENDATION_IMPRESSION_THRESHOLD,
                    settleTime = PRODUCT_RECOMMENDATION_SETTLE_TIME_MS,
                    onVisible = { onImpression(product.aid) },
                ),
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    text = stringResource(R.string.review_quality_check_ad_title),
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.headline8,
                )

                Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    Image(
                        url = product.imageUrl,
                        modifier = Modifier.size(productRecommendationImageSize),
                        targetSize = productRecommendationImageSize,
                        placeholder = { ImagePlaceholder() },
                        fallback = { ImagePlaceholder() },
                    )

                    Text(
                        text = product.name,
                        modifier = Modifier.weight(1.0f),
                        color = FirefoxTheme.colors.textAccent,
                        textDecoration = TextDecoration.Underline,
                        style = FirefoxTheme.typography.body2,
                    )

                    ReviewGradeCompact(grade = product.reviewGrade)
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text(
                        text = product.formattedPrice,
                        color = FirefoxTheme.colors.textPrimary,
                        style = FirefoxTheme.typography.headline8,
                    )

                    StarRating(value = product.adjustedRating)
                }
            }
        }

        Text(
            text = stringResource(
                id = R.string.review_quality_check_ad_caption,
                stringResource(id = R.string.shopping_product_name),
            ),
            color = FirefoxTheme.colors.textSecondary,
            style = FirefoxTheme.typography.body2,
        )
    }
}

@Composable
private fun ImagePlaceholder() {
    Box(
        modifier = Modifier
            .size(productRecommendationImageSize)
            .background(
                color = FirefoxTheme.colors.layer3,
                shape = RoundedCornerShape(8.dp),
            ),
    ) {
        Image(
            painter = painterResource(id = R.drawable.ic_file_type_image),
            contentDescription = null,
            modifier = Modifier
                .align(Alignment.Center),
        )
    }
}

private class ProductAnalysisPreviewModel(
    val productRecommendationsEnabled: Boolean?,
    val productAnalysis: AnalysisPresent,
    val productVendor: ReviewQualityCheckState.ProductVendor,
) {
    constructor(
        productRecommendationsEnabled: Boolean? = false,
        productId: String = "123",
        reviewGrade: ReviewQualityCheckState.Grade? = ReviewQualityCheckState.Grade.B,
        analysisStatus: AnalysisStatus = AnalysisStatus.UpToDate,
        adjustedRating: Float? = 3.6f,
        productUrl: String = "",
        highlightsInfo: HighlightsInfo = HighlightsInfo(
            mapOf(
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
        recommendedProductState: RecommendedProductState = RecommendedProductState.Initial,
        productVendor: ReviewQualityCheckState.ProductVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
    ) : this(
        productRecommendationsEnabled = productRecommendationsEnabled,
        productAnalysis = AnalysisPresent(
            productId = productId,
            reviewGrade = reviewGrade,
            analysisStatus = analysisStatus,
            adjustedRating = adjustedRating,
            productUrl = productUrl,
            highlightsInfo = highlightsInfo,
            recommendedProductState = recommendedProductState,
        ),
        productVendor = productVendor,
    )
}

private class ProductAnalysisPreviewModelParameterProvider :
    PreviewParameterProvider<ProductAnalysisPreviewModel> {
    override val values: Sequence<ProductAnalysisPreviewModel>
        get() = sequenceOf(
            ProductAnalysisPreviewModel(),
            ProductAnalysisPreviewModel(
                analysisStatus = AnalysisStatus.NeedsAnalysis,
            ),
            ProductAnalysisPreviewModel(
                analysisStatus = AnalysisStatus.Reanalyzing(
                    ReviewQualityCheckState.OptedIn.ProductReviewState.Progress(40f),
                ),
            ),
            ProductAnalysisPreviewModel(
                analysisStatus = AnalysisStatus.Reanalyzing(
                    ReviewQualityCheckState.OptedIn.ProductReviewState.Progress(95f),
                ),
            ),
            ProductAnalysisPreviewModel(
                reviewGrade = null,
            ),
            ProductAnalysisPreviewModel(
                highlightsInfo = HighlightsInfo(
                    mapOf(
                        HighlightType.QUALITY to listOf(
                            "High quality",
                            "Excellent craftsmanship",
                        ),
                    ),
                ),
            ),
            ProductAnalysisPreviewModel(
                productRecommendationsEnabled = true,
                recommendedProductState = RecommendedProductState.Product(
                    aid = "aid",
                    name = "The best desk ever with a really really really long product name that " +
                        "forces the preview to wrap its text to at least 4 lines.",
                    productUrl = "www.mozilla.com",
                    imageUrl = "https://i.fakespot.io/b6vx27xf3rgwr1a597q6qd3rutp6",
                    formattedPrice = "$123.45",
                    reviewGrade = ReviewQualityCheckState.Grade.B,
                    adjustedRating = 4.23f,
                    isSponsored = true,
                    analysisUrl = "",
                ),
            ),
        )
}

@Composable
@LightDarkPreview
private fun ProductAnalysisPreview(
    @PreviewParameter(ProductAnalysisPreviewModelParameterProvider::class) model: ProductAnalysisPreviewModel,
) {
    FirefoxTheme {
        ReviewQualityCheckScaffold(
            onRequestDismiss = {},
        ) {
            var productRecommendationsEnabled by remember { mutableStateOf(model.productRecommendationsEnabled) }
            var isSettingsExpanded by remember { mutableStateOf(false) }
            var isInfoExpanded by remember { mutableStateOf(false) }
            var isHighlightsExpanded by remember { mutableStateOf(false) }

            ProductAnalysis(
                productRecommendationsEnabled = productRecommendationsEnabled,
                productAnalysis = model.productAnalysis,
                productVendor = model.productVendor,
                isSettingsExpanded = isSettingsExpanded,
                isInfoExpanded = isInfoExpanded,
                isHighlightsExpanded = isHighlightsExpanded,
                onOptOutClick = {},
                onReanalyzeClick = {},
                onProductRecommendationsEnabledStateChange = {
                    productRecommendationsEnabled = it
                },
                onReviewGradeLearnMoreClick = {},
                onFooterLinkClick = {},
                onHighlightsExpandToggleClick = { isHighlightsExpanded = !isHighlightsExpanded },
                onSettingsExpandToggleClick = { isSettingsExpanded = !isSettingsExpanded },
                onInfoExpandToggleClick = { isInfoExpanded = !isInfoExpanded },
                onRecommendedProductClick = { _, _ -> },
                onRecommendedProductImpression = {},
            )
        }
    }
}
