/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Top-level UI for displaying the Tabs Tray feature.
 */
@Composable
fun TabsTray() {
    Box(
        modifier = Modifier.fillMaxSize()
            .background(FirefoxTheme.colors.layer1),
    ) {
        Text(
            text = "Tabs Tray to Compose",
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body1,
        )
    }
}

@LightDarkPreview
@Composable
private fun TabsTrayPreview() {
    FirefoxTheme {
        TabsTray()
    }
}
