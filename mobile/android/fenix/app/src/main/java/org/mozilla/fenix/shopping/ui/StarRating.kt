/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.max
import kotlin.math.min
import kotlin.math.round
import kotlin.math.roundToInt

private const val NUM_STARS = 5
private const val MAX_RATING = 5f
private const val MIN_RATING = 0f

/**
 * UI for displaying star rating bar with maximum 5 stars.
 *
 * @param value The rating to be displayed as filled stars.
 * @param modifier The modifier to be applied to the composable.
 */
@Composable
fun StarRating(
    value: Float,
    modifier: Modifier = Modifier,
) {
    val rating: Float = remember(value) {
        max(min(MAX_RATING, value), MIN_RATING).roundToNearestHalf()
    }
    val contentDescription = contentDescription(rating = value)

    Row(
        modifier = modifier.semantics {
            this.contentDescription = contentDescription
        },
        horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        repeat(NUM_STARS) {
            val starId = if (it < rating && it + 1 > rating) {
                R.drawable.ic_star_half
            } else if (it < rating) {
                R.drawable.ic_bookmark_filled
            } else {
                R.drawable.ic_bookmark_outline
            }

            Image(
                painter = painterResource(id = starId),
                colorFilter = ColorFilter.tint(FirefoxTheme.colors.iconPrimary),
                contentDescription = null,
            )
        }
    }
}

@Composable
private fun contentDescription(rating: Float): String {
    val formattedRating: Number = remember(rating) { rating.removeDecimalZero() }

    return stringResource(
        R.string.review_quality_check_star_rating_content_description,
        formattedRating,
    )
}

/**
 * Removes decimal zero if present and returns an Int, otherwise returns the same float as Number.
 * e.g.4.0 becomes 4, 3.4 stays 3.4.
 */
@VisibleForTesting
fun Float.removeDecimalZero(): Number =
    if (this % 1 == 0f) {
        roundToInt()
    } else {
        this
    }

/**
 * Rounds the float to the nearest half instead of the whole number.
 * e.g 4.6 becomes 4.5 instead of 5.
 */
@VisibleForTesting
fun Float.roundToNearestHalf(): Float =
    round(this * 2) / 2f

@LightDarkPreview
@Composable
@Suppress("MagicNumber")
private fun StarRatingPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            listOf(0.4f, 0.9f, 1.6f, 2.2f, 3f, 3.6f, 4.1f, 4.65f, 4.9f).forEach {
                StarRating(value = it)
            }
        }
    }
}
