/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Top-level UI for displaying the banner in [TabsTray].
 *
 * @param isInMultiSelectMode Whether the tab list is in multi-select mode.
 * @param onTabPageIndicatorClicked Invoked when the user clicks on a tab page indicator.
 */
@Composable
fun TabsTrayBanner(
    isInMultiSelectMode: Boolean,
    onTabPageIndicatorClicked: (Page) -> Unit,
) {
    if (isInMultiSelectMode) {
        MultiSelectBanner()
    } else {
        SingleSelectBanner(
            onTabPageIndicatorClicked = onTabPageIndicatorClicked,
        )
    }
}

@Composable
private fun SingleSelectBanner(
    onTabPageIndicatorClicked: (Page) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(80.dp)
            .padding(all = 16.dp),
        horizontalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Box(modifier = Modifier.weight(1.0f)) {
            PrimaryButton(text = "Normal tabs") {
                onTabPageIndicatorClicked(Page.NormalTabs)
            }
        }

        Box(modifier = Modifier.weight(1.0f)) {
            PrimaryButton(text = "Private tabs") {
                onTabPageIndicatorClicked(Page.PrivateTabs)
            }
        }

        Box(modifier = Modifier.weight(1.0f)) {
            PrimaryButton(text = "Synced tabs") {
                onTabPageIndicatorClicked(Page.SyncedTabs)
            }
        }
    }
}

@Composable
private fun MultiSelectBanner() {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(80.dp)
            .background(color = FirefoxTheme.colors.layerAccent),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = "Multi selection mode",
            color = FirefoxTheme.colors.textOnColorPrimary,
            style = FirefoxTheme.typography.body1,
        )
    }
}

@LightDarkPreview
@Composable
private fun TabsTrayBannerPreview() {
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            TabsTrayBanner(
                isInMultiSelectMode = false,
                onTabPageIndicatorClicked = {},
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun TabsTrayBannerMultiselectPreview() {
    FirefoxTheme {
        TabsTrayBanner(
            isInMultiSelectMode = true,
            onTabPageIndicatorClicked = {},
        )
    }
}
