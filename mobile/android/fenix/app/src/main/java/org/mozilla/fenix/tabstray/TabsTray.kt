/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.google.accompanist.pager.ExperimentalPagerApi
import com.google.accompanist.pager.HorizontalPager
import com.google.accompanist.pager.rememberPagerState
import kotlinx.coroutines.launch
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Top-level UI for displaying the Tabs Tray feature.
 */
@OptIn(ExperimentalPagerApi::class)
@Suppress("LongMethod")
@Composable
fun TabsTray() {
    val pagerState = rememberPagerState(initialPage = 0)
    val scope = rememberCoroutineScope()
    val animateScrollToPage: ((Int) -> Unit) = { pagePosition ->
        scope.launch {
            pagerState.animateScrollToPage(pagePosition)
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(FirefoxTheme.colors.layer1),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(all = 16.dp),
        ) {
            Text(
                text = "Temporary tab switchers",
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.body1,
            )

            Spacer(modifier = Modifier.height(16.dp))

            PrimaryButton(text = "Normal tabs") {
                animateScrollToPage(Page.NormalTabs.ordinal)
            }

            Spacer(modifier = Modifier.height(16.dp))

            PrimaryButton(text = "Private tabs") {
                animateScrollToPage(Page.PrivateTabs.ordinal)
            }

            Spacer(modifier = Modifier.height(16.dp))

            PrimaryButton(text = "Synced tabs") {
                animateScrollToPage(Page.SyncedTabs.ordinal)
            }
        }

        Divider()

        Box(modifier = Modifier.fillMaxSize()) {
            HorizontalPager(
                count = Page.values().size,
                modifier = Modifier.fillMaxSize(),
                state = pagerState,
                userScrollEnabled = false,
            ) { position ->
                when (Page.positionToPage(position)) {
                    Page.NormalTabs -> {
                        Text(
                            text = "Normal tabs",
                            modifier = Modifier.padding(all = 16.dp),
                            color = FirefoxTheme.colors.textPrimary,
                            style = FirefoxTheme.typography.body1,
                        )
                    }
                    Page.PrivateTabs -> {
                        Text(
                            text = "Private tabs",
                            modifier = Modifier.padding(all = 16.dp),
                            color = FirefoxTheme.colors.textPrimary,
                            style = FirefoxTheme.typography.body1,
                        )
                    }
                    Page.SyncedTabs -> {
                        Text(
                            text = "Synced tabs",
                            modifier = Modifier.padding(all = 16.dp),
                            color = FirefoxTheme.colors.textPrimary,
                            style = FirefoxTheme.typography.body1,
                        )
                    }
                }
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun TabsTrayPreview() {
    FirefoxTheme {
        TabsTray()
    }
}
