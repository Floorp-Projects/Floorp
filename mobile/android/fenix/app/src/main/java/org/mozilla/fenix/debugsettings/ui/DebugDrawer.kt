/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The debug drawer UI.
 */
@Composable
fun DebugDrawer() {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(color = FirefoxTheme.colors.layer1),
    ) {
        Text(text = "Debug drawer")
    }
}

@Composable
@LightDarkPreview
private fun DebugDrawerPreview() {
    FirefoxTheme {
        DebugDrawer()
    }
}
