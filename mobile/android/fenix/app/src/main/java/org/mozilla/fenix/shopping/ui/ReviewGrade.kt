/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.foundation.shape.CornerSize
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.Grade
import org.mozilla.fenix.theme.FirefoxTheme

private val height = 24.dp
private val borderColor = Color(0x26000000)
private val reviewGradeAColor = PhotonColors.Green20
private val reviewGradeBColor = PhotonColors.Blue10
private val reviewGradeCColor = PhotonColors.Yellow20
private val reviewGradeDColor = PhotonColors.Orange20
private val reviewGradeFColor = PhotonColors.Red30
private val reviewGradeAColorExpanded = Color(0xFFEEFFF9)
private val reviewGradeBColorExpanded = Color(0xFFDEFAFF)
private val reviewGradeCColorExpanded = Color(0xFFFFF9DA)
private val reviewGradeDColorExpanded = Color(0xFFFDEEE2)
private val reviewGradeFColorExpanded = Color(0xFFFFEFF0)

/**
 * Review Grade of the product - A being the best and F being the worst.
 */
private enum class ReviewGrade(
    val stringResourceId: Int,
    val backgroundColor: Color,
    val expandedTextBackgroundColor: Color,
) {
    A(
        stringResourceId = R.string.review_quality_check_grade_a_b_description,
        backgroundColor = reviewGradeAColor,
        expandedTextBackgroundColor = reviewGradeAColorExpanded,
    ),
    B(
        stringResourceId = R.string.review_quality_check_grade_a_b_description,
        backgroundColor = reviewGradeBColor,
        expandedTextBackgroundColor = reviewGradeBColorExpanded,
    ),
    C(
        stringResourceId = R.string.review_quality_check_grade_c_description,
        backgroundColor = reviewGradeCColor,
        expandedTextBackgroundColor = reviewGradeCColorExpanded,
    ),
    D(
        stringResourceId = R.string.review_quality_check_grade_d_f_description,
        backgroundColor = reviewGradeDColor,
        expandedTextBackgroundColor = reviewGradeDColorExpanded,
    ),
    F(
        stringResourceId = R.string.review_quality_check_grade_d_f_description,
        backgroundColor = reviewGradeFColor,
        expandedTextBackgroundColor = reviewGradeFColorExpanded,
    ),
}

/**
 * UI for displaying the review grade.
 *
 * @param modifier The modifier to be applied to the Composable.
 * @param grade The grade of the product.
 */
@Composable
fun ReviewGradeCompact(
    modifier: Modifier = Modifier,
    grade: Grade,
) {
    ReviewGradeLetter(
        reviewGrade = grade.toReviewGrade(),
        modifier = modifier.border(
            border = BorderStroke(
                width = 1.dp,
                color = borderColor,
            ),
            shape = MaterialTheme.shapes.small,
        ),
    )
}

/**
 * UI for displaying the review grade with descriptive text.
 *
 * @param modifier The modifier to be applied to the Composable.
 * @param grade The grade of the product.
 */
@Composable
fun ReviewGradeExpanded(
    modifier: Modifier = Modifier,
    grade: Grade,
) {
    val reviewGrade = grade.toReviewGrade()

    Row(
        modifier = modifier
            .background(
                color = reviewGrade.expandedTextBackgroundColor,
                shape = MaterialTheme.shapes.small,
            )
            .border(
                border = BorderStroke(
                    width = 1.dp,
                    color = borderColor,
                ),
                shape = MaterialTheme.shapes.small,
            ),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val cornerSize = CornerSize(0.dp)
        val shape =
            MaterialTheme.shapes.small.copy(topEnd = cornerSize, bottomEnd = cornerSize)

        ReviewGradeLetter(
            reviewGrade = reviewGrade,
            shape = shape,
        )

        Text(
            text = stringResource(id = reviewGrade.stringResourceId),
            color = PhotonColors.DarkGrey90,
            style = FirefoxTheme.typography.caption,
            modifier = Modifier.padding(horizontal = 8.dp),
        )
    }
}

/**
 * Common UI building block for the review grade.
 */
@Composable
private fun ReviewGradeLetter(
    modifier: Modifier = Modifier,
    reviewGrade: ReviewGrade,
    shape: Shape = MaterialTheme.shapes.small,
) {
    Box(
        modifier = modifier
            .size(height)
            .background(
                color = reviewGrade.backgroundColor,
                shape = shape,
            )
            .wrapContentSize(Alignment.Center),
    ) {
        Text(
            text = reviewGrade.name,
            color = PhotonColors.Black,
            style = FirefoxTheme.typography.subtitle2,
        )
    }
}

/**
 * Maps [Grade] to [ReviewGrade].
 */
private fun Grade.toReviewGrade(): ReviewGrade =
    when (this) {
        Grade.A -> ReviewGrade.A
        Grade.B -> ReviewGrade.B
        Grade.C -> ReviewGrade.C
        Grade.D -> ReviewGrade.D
        Grade.F -> ReviewGrade.F
    }

@Composable
@LightDarkPreview
private fun ReviewGradePreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(16.dp),
        ) {
            Grade.values().forEach {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(32.dp),
                ) {
                    ReviewGradeCompact(grade = it)

                    ReviewGradeExpanded(grade = it)
                }

                Spacer(modifier = Modifier.height(16.dp))
            }
        }
    }
}
