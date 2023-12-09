/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Snackbar
import androidx.compose.material.SnackbarHost
import androidx.compose.material.SnackbarHostState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.FloatingActionButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Overlay for presenting Fenix-wide debugging content.
 */
@Composable
fun DebugOverlay() {
    val snackbarState = remember { SnackbarHostState() }
    val scope = rememberCoroutineScope()

    Box(
        modifier = Modifier.fillMaxSize(),
    ) {
        FloatingActionButton(
            icon = painterResource(R.drawable.ic_new),
            modifier = Modifier
                .align(Alignment.CenterStart)
                .padding(start = 16.dp),
            onClick = {
                scope.launch {
                    snackbarState.showSnackbar("Show debug drawer")
                }
            },
        )

        // This must be the last element in the Box
        SnackbarHost(
            hostState = snackbarState,
            modifier = Modifier.align(Alignment.BottomCenter),
        ) { snackbarData ->
            Snackbar(
                snackbarData = snackbarData,
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun DebugOverlayPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(color = FirefoxTheme.colors.layer1),
        ) {
            DebugOverlay()
        }
    }
}
