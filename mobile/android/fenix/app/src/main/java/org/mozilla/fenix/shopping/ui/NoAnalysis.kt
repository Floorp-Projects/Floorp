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
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * No analysis UI for review quality check content.
 *
 * @param isAnalyzing Whether or not the displayed product is being analyzed.
 * @param productRecommendationsEnabled The current state of the product recommendations toggle.
 * @param onAnalyzeClick Invoked when the user clicks on the check review button.
 * @param onReviewGradeLearnMoreClick Invoked when the user clicks to learn more about review grades.
 * @param onOptOutClick Invoked when the user opts out of the review quality check feature.
 * @param onProductRecommendationsEnabledStateChange Invoked when the user changes the product
 * recommendations toggle state.
 * @param modifier Modifier to be applied to the composable.
 */
@Suppress("LongParameterList")
@Composable
fun NoAnalysis(
    isAnalyzing: Boolean,
    productRecommendationsEnabled: Boolean?,
    onAnalyzeClick: () -> Unit,
    onReviewGradeLearnMoreClick: (String) -> Unit,
    onOptOutClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        ReviewQualityNoAnalysisCard(isAnalyzing, onAnalyzeClick)

        ReviewQualityInfoCard(
            onLearnMoreClick = onReviewGradeLearnMoreClick,
        )

        ReviewQualityCheckSettingsCard(
            productRecommendationsEnabled = productRecommendationsEnabled,
            onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
            onTurnOffReviewQualityCheckClick = onOptOutClick,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

@Composable
private fun ReviewQualityNoAnalysisCard(
    isAnalyzing: Boolean,
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

        if (isAnalyzing) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
            ) {
                IndeterminateProgressIndicator(
                    Modifier.size(24.dp),
                )

                Spacer(modifier = Modifier.width(8.dp))

                Text(
                    text = stringResource(id = R.string.review_quality_check_analysis_in_progress_warning_title),
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

@Composable
@LightDarkPreview
private fun NoAnalysisPreview() {
    var isAnalyzing by remember { mutableStateOf(false) }

    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(color = FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            NoAnalysis(
                isAnalyzing = isAnalyzing,
                onAnalyzeClick = { isAnalyzing = !isAnalyzing },
                productRecommendationsEnabled = false,
                onReviewGradeLearnMoreClick = {},
                onOptOutClick = {},
                onProductRecommendationsEnabledStateChange = {},
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
