/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Review quality check settings card UI. Contains toggles to disable product recommendations and
 * the entire review quality check feature.
 *
 * @param productRecommendationsEnabled The current state of the product recommendations toggle.
 * @param onProductRecommendationsEnabledStateChange Invoked when the user changes the product
 * recommendations toggle state.
 * @param onTurnOffReviewQualityCheckClick Invoked when the user opts out of the review quality check feature.
 * @param modifier Modifier to apply to the layout.
 */
@Composable
fun ReviewQualityCheckSettingsCard(
    productRecommendationsEnabled: Boolean,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onTurnOffReviewQualityCheckClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    ReviewQualityCheckExpandableCard(
        modifier = modifier,
        title = stringResource(R.string.review_quality_check_settings_title),
    ) {
        SettingsContent(
            productRecommendationsEnabled = productRecommendationsEnabled,
            onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
            onTurnOffReviewQualityCheckClick = onTurnOffReviewQualityCheckClick,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

@Composable
private fun SettingsContent(
    productRecommendationsEnabled: Boolean,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onTurnOffReviewQualityCheckClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier) {
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

@LightDarkPreview
@Composable
private fun ReviewQualityCheckSettingsCardPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(color = FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            ReviewQualityCheckSettingsCard(
                productRecommendationsEnabled = true,
                onProductRecommendationsEnabledStateChange = {},
                onTurnOffReviewQualityCheckClick = {},
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun SettingsContentPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(color = FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            SettingsContent(
                productRecommendationsEnabled = true,
                onProductRecommendationsEnabledStateChange = {},
                onTurnOffReviewQualityCheckClick = {},
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
