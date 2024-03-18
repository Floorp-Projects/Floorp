/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.NoAnalysisPresent
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.Progress
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * No analysis UI for review quality check content.
 *
 * @param noAnalysisPresent The state of the analysis progress.
 * @param productRecommendationsEnabled The current state of the product recommendations toggle.
 * @param productVendor The vendor of the product.
 * @param isSettingsExpanded Whether or not the settings card is expanded.
 * @param isInfoExpanded Whether or not the info card is expanded.
 * @param onAnalyzeClick Invoked when the user clicks on the check review button.
 * @param onReviewGradeLearnMoreClick Invoked when the user clicks to learn more about review grades.
 * @param onOptOutClick Invoked when the user opts out of the review quality check feature.
 * @param onProductRecommendationsEnabledStateChange Invoked when the user changes the product
 * recommendations toggle state.
 * @param onSettingsExpandToggleClick Invoked when the user expands or collapses the settings card.
 * @param onInfoExpandToggleClick Invoked when the user expands or collapses the info card.
 * @param onFooterLinkClick Invoked when the user clicks on the footer link.
 * @param modifier Modifier to be applied to the composable.
 */
@Suppress("LongParameterList")
@Composable
fun NoAnalysis(
    noAnalysisPresent: NoAnalysisPresent,
    productRecommendationsEnabled: Boolean?,
    productVendor: ReviewQualityCheckState.ProductVendor,
    isSettingsExpanded: Boolean,
    isInfoExpanded: Boolean,
    onAnalyzeClick: () -> Unit,
    onReviewGradeLearnMoreClick: () -> Unit,
    onOptOutClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onSettingsExpandToggleClick: () -> Unit,
    onInfoExpandToggleClick: () -> Unit,
    onFooterLinkClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        ReviewQualityNoAnalysisCard(noAnalysisPresent, onAnalyzeClick)

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
private fun ReviewQualityNoAnalysisCard(
    noAnalysisPresent: NoAnalysisPresent,
    onAnalyzeClick: () -> Unit,
) {
    ReviewQualityCheckCard(
        modifier = Modifier.fillMaxWidth(),
    ) {
        Image(
            painter = painterResource(id = R.drawable.shopping_no_analysis),
            contentDescription = null,
            modifier = Modifier
                .fillMaxWidth()
                .wrapContentHeight()
                .padding(all = 10.dp),
        )

        Spacer(Modifier.height(8.dp))

        if (noAnalysisPresent.isProgressBarVisible) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
            ) {
                DeterminateProgressIndicator(
                    progress = noAnalysisPresent.progress.normalizedProgress,
                    modifier = Modifier.size(24.dp),
                )

                Spacer(modifier = Modifier.width(8.dp))

                Text(
                    text = stringResource(
                        id = R.string.review_quality_check_analysis_in_progress_warning_title_2,
                        noAnalysisPresent.progress.formattedProgress,
                    ),
                    style = FirefoxTheme.typography.headline8,
                    color = FirefoxTheme.colors.textPrimary,
                    modifier = Modifier.fillMaxWidth(),
                )
            }

            Spacer(Modifier.height(8.dp))

            Text(
                text = stringResource(id = R.string.review_quality_check_analysis_in_progress_warning_body),
                style = FirefoxTheme.typography.body2,
                color = FirefoxTheme.colors.textSecondary,
                modifier = Modifier.fillMaxWidth(),
            )
        } else {
            Text(
                text = stringResource(id = R.string.review_quality_check_no_analysis_title),
                style = FirefoxTheme.typography.headline8,
                color = FirefoxTheme.colors.textPrimary,
                modifier = Modifier.fillMaxWidth(),
            )

            Spacer(Modifier.height(8.dp))

            Text(
                text = stringResource(id = R.string.review_quality_check_no_analysis_body),
                style = FirefoxTheme.typography.body2,
                color = FirefoxTheme.colors.textSecondary,
                modifier = Modifier.fillMaxWidth(),
            )

            Spacer(Modifier.height(8.dp))

            TextButton(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(
                        color = FirefoxTheme.colors.actionPrimary,
                        shape = RoundedCornerShape(4.dp),
                    ),
                onClick = onAnalyzeClick,
            ) {
                Text(
                    text = stringResource(id = R.string.review_quality_check_no_analysis_link),
                    color = FirefoxTheme.colors.textOnColorPrimary,
                    style = FirefoxTheme.typography.headline8,
                )
            }
        }
    }
}

private class NoAnalysisPreviewModelParameterProvider :
    PreviewParameterProvider<NoAnalysisPresent> {
    override val values: Sequence<NoAnalysisPresent>
        get() = sequenceOf(
            NoAnalysisPresent(),
            NoAnalysisPresent(Progress(50f)),
            NoAnalysisPresent(Progress(100f)),
        )
}

@Composable
@LightDarkPreview
private fun NoAnalysisPreview(
    @PreviewParameter(NoAnalysisPreviewModelParameterProvider::class) noAnalysisPresent: NoAnalysisPresent,
) {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(color = FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            var isAnalyzing by remember { mutableStateOf(false) }
            var productRecommendationsEnabled by remember { mutableStateOf(false) }
            var isSettingsExpanded by remember { mutableStateOf(false) }
            var isInfoExpanded by remember { mutableStateOf(false) }

            NoAnalysis(
                noAnalysisPresent = noAnalysisPresent,
                onAnalyzeClick = { isAnalyzing = !isAnalyzing },
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                productRecommendationsEnabled = productRecommendationsEnabled,
                isSettingsExpanded = isSettingsExpanded,
                isInfoExpanded = isInfoExpanded,
                onReviewGradeLearnMoreClick = {},
                onOptOutClick = {},
                onProductRecommendationsEnabledStateChange = { productRecommendationsEnabled = it },
                onSettingsExpandToggleClick = { isSettingsExpanded = !isSettingsExpanded },
                onInfoExpandToggleClick = { isInfoExpanded = !isInfoExpanded },
                onFooterLinkClick = {},
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
