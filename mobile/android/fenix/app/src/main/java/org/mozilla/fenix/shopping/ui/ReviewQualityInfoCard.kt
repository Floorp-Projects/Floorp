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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.parseHtml
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.ui.ext.displayName
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Info card UI containing an explanation of the review quality.
 *
 * @param productVendor The vendor of the product.
 * @param modifier Modifier to apply to the layout.
 * @param onLearnMoreClick Invoked when the user clicks to learn more about review grades.
 */
@Composable
fun ReviewQualityInfoCard(
    productVendor: ReviewQualityCheckState.ProductVendor,
    modifier: Modifier = Modifier,
    onLearnMoreClick: () -> Unit,
) {
    ReviewQualityCheckExpandableCard(
        title = stringResource(id = R.string.review_quality_check_explanation_title),
        modifier = modifier,
    ) {
        ReviewQualityInfo(
            productVendor = productVendor,
            modifier = Modifier.fillMaxWidth(),
            onLearnMoreClick = onLearnMoreClick,
        )
    }
}

@Suppress("LongMethod")
@Composable
private fun ReviewQualityInfo(
    productVendor: ReviewQualityCheckState.ProductVendor,
    modifier: Modifier = Modifier,
    onLearnMoreClick: () -> Unit,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(24.dp),
    ) {
        val letterGradeText =
            stringResource(id = R.string.review_quality_check_info_review_grade_header)
        val adjustedGradingText =
            stringResource(id = R.string.review_quality_check_explanation_body_adjusted_grading)
        val highlightsText = stringResource(
            id = R.string.review_quality_check_explanation_body_highlights,
            productVendor.displayName(),
        )

        Text(
            text = stringResource(
                id = R.string.review_quality_check_explanation_body_reliability,
                stringResource(R.string.shopping_product_name),
            ),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )

        Text(
            text = remember(letterGradeText) { parseHtml(letterGradeText) },
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
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

        Text(
            text = remember(highlightsText) { parseHtml(highlightsText) },
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )

        val link = stringResource(
            id = R.string.review_quality_check_info_learn_more_link_2,
            stringResource(R.string.shopping_product_name),
        )
        val text = stringResource(R.string.review_quality_check_info_learn_more, link)
        LinkText(
            text = text,
            linkTextStates = listOf(
                LinkTextState(
                    text = link,
                    url = "",
                    onClick = {
                        onLearnMoreClick()
                    },
                ),
            ),
            style = FirefoxTheme.typography.body2.copy(
                color = FirefoxTheme.colors.textPrimary,
            ),
            linkTextColor = FirefoxTheme.colors.textAccent,
            linkTextDecoration = TextDecoration.Underline,
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
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
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
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                modifier = Modifier.fillMaxWidth(),
                onLearnMoreClick = {},
            )
        }
    }
}
