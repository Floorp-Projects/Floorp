/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.debugsettings.navigation.DebugDrawerDestination
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The debug drawer UI.
 *
 * @param navController [NavHostController] used to perform navigation actions on the [NavHost].
 * @param destinations The list of [DebugDrawerDestination]s (excluding home) used to populate
 * the [NavHost] with screens.
 * @param onBackButtonClick Invoked when the user taps on the back button in the app bar.
 */
@Composable
fun DebugDrawer(
    navController: NavHostController,
    destinations: List<DebugDrawerDestination>,
    onBackButtonClick: () -> Unit,
) {
    var backButtonVisible by remember { mutableStateOf(false) }
    var toolbarTitle by remember { mutableStateOf("") }

    Column(modifier = Modifier.fillMaxSize()) {
        TopAppBar(
            title = {
                Text(
                    text = toolbarTitle,
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.headline6,
                )
            },
            navigationIcon = if (backButtonVisible) {
                topBarBackButton(onClick = onBackButtonClick)
            } else {
                null
            },
            backgroundColor = FirefoxTheme.colors.layer1,
            elevation = 5.dp,
        )

        NavHost(
            navController = navController,
            startDestination = DEBUG_DRAWER_HOME_ROUTE,
            modifier = Modifier.fillMaxSize(),
        ) {
            composable(route = DEBUG_DRAWER_HOME_ROUTE) {
                toolbarTitle = stringResource(id = R.string.debug_drawer_title)
                backButtonVisible = false
                DebugDrawerHome(destinations = destinations)
            }

            destinations.forEach { destination ->
                composable(route = destination.route) {
                    toolbarTitle = stringResource(id = destination.title)
                    backButtonVisible = true

                    destination.content()
                }
            }
        }
    }
}

@Composable
private fun topBarBackButton(onClick: () -> Unit): @Composable () -> Unit = {
    IconButton(
        onClick = onClick,
    ) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_back_24),
            contentDescription = stringResource(R.string.debug_drawer_back_button_content_description),
            tint = FirefoxTheme.colors.iconPrimary,
        )
    }
}

@Composable
@LightDarkPreview
private fun DebugDrawerPreview() {
    val navController = rememberNavController()
    val destinations = remember {
        List(size = 15) { index ->
            DebugDrawerDestination(
                route = "screen_$index",
                title = R.string.debug_drawer_title,
                onClick = {
                    navController.navigate(route = "screen_$index")
                },
                content = {
                    Text(
                        text = "Tool $index",
                        color = FirefoxTheme.colors.textPrimary,
                        style = FirefoxTheme.typography.headline6,
                    )
                },
            )
        }
    }

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            DebugDrawer(
                navController = navController,
                destinations = destinations,
                onBackButtonClick = {
                    navController.popBackStack()
                },
            )
        }
    }
}
