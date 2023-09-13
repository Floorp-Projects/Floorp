/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ClickableSubstringLink
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.parseHtml
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Info card UI containing an explanation of the review quality.
 *
 * @param modifier Modifier to apply to the layout.
 * @param onLearnMoreClick Invoked when the user clicks to learn more about review grades.
 */
@Composable
fun ReviewQualityInfoCard(
    modifier: Modifier = Modifier,
    onLearnMoreClick: (String) -> Unit,
) {
    ReviewQualityCheckExpandableCard(
        title = stringResource(id = R.string.review_quality_check_explanation_title),
        modifier = modifier,
    ) {
        ReviewQualityInfo(
            modifier = Modifier.fillMaxWidth(),
            onLearnMoreClick = onLearnMoreClick,
        )
    }
}

@Suppress("Deprecation")
@Composable
private fun ReviewQualityInfo(
    modifier: Modifier = Modifier,
    onLearnMoreClick: (String) -> Unit,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(24.dp),
    ) {
        val adjustedGradingText =
            stringResource(id = R.string.review_quality_check_explanation_body_adjusted_grading)
        // Any and all text formatting (bullets, inline substring bolding, etc.) will be handled as
        // follow-up when the copy is finalized.
        // Bug 1848219
        Text(
            text = stringResource(
                id = R.string.review_quality_check_explanation_body_reliability,
                stringResource(R.string.shopping_product_name),
            ),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )

        val link = stringResource(
            id = R.string.review_quality_check_info_learn_more_link,
            stringResource(R.string.shopping_product_name),
        )
        val text = stringResource(R.string.review_quality_check_info_learn_more, link)
        val context = LocalContext.current
        val linkStartIndex = text.indexOf(link)
        val linkEndIndex = linkStartIndex + link.length
        ClickableSubstringLink(
            text = text,
            textStyle = FirefoxTheme.typography.body2,
            clickableStartIndex = linkStartIndex,
            clickableEndIndex = linkEndIndex,
            onClick = {
                onLearnMoreClick(
                    // Placeholder Sumo page
                    SupportUtils.getSumoURLForTopic(
                        context,
                        SupportUtils.SumoTopic.HELP,
                    ),
                )
            },
        )

        ReviewGradingScaleInfo(
            reviewGrades = listOf(
                ReviewQualityCheckState.Grade.A,
                ReviewQualityCheckState.Grade.B,
            ),
            info = stringResource(id = R.string.review_quality_check_info_grade_info_AB),
            modifier = Modifier.fillMaxWidth(),
        )

        ReviewGradingScaleInfo(
            reviewGrades = listOf(ReviewQualityCheckState.Grade.C),
            info = stringResource(id = R.string.review_quality_check_info_grade_info_C),
            modifier = Modifier.fillMaxWidth(),
        )

        ReviewGradingScaleInfo(
            reviewGrades = listOf(
                ReviewQualityCheckState.Grade.D,
                ReviewQualityCheckState.Grade.F,
            ),
            info = stringResource(id = R.string.review_quality_check_info_grade_info_DF),
            modifier = Modifier.fillMaxWidth(),
        )

        Text(
            text = remember(adjustedGradingText) { parseHtml(adjustedGradingText) },
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )
    }
}

@Composable
private fun ReviewGradingScaleInfo(
    reviewGrades: List<ReviewQualityCheckState.Grade>,
    info: String,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier.semantics(mergeDescendants = true) {},
        verticalAlignment = Alignment.Top,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        reviewGrades.forEach { grade ->
            ReviewGradeCompact(grade = grade)
        }

        if (reviewGrades.size == 1) {
            Spacer(modifier = Modifier.width(24.dp))
        }

        Text(
            text = info,
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )
    }
}

@Composable
@LightDarkPreview
private fun ReviewQualityInfoCardPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(color = FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            ReviewQualityInfoCard(
                modifier = Modifier.fillMaxWidth(),
                onLearnMoreClick = {},
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun ReviewQualityInfoPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(color = FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            ReviewQualityInfo(
                modifier = Modifier.fillMaxWidth(),
                onLearnMoreClick = {},
            )
        }
    }
}
