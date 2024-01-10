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
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The debug drawer UI.
 *
 * @param navController [NavHostController] used to perform navigation actions on the [NavHost].
 * @param onBackButtonClick Invoked when the user taps on the back button in the app bar.
 */
@Composable
fun DebugDrawer(
    navController: NavHostController,
    onBackButtonClick: () -> Unit,
) {
    var backButtonVisible by remember { mutableStateOf(false) }
    var toolbarTitle by remember { mutableStateOf("") }

    // This is temporary until https://bugzilla.mozilla.org/show_bug.cgi?id=1864076
    val homeMenuItems = List(size = 5) {
        DebugDrawerMenuItem(
            label = "Screen $it",
            onClick = {
                navController.navigate("screen_$it")
            },
        )
    }

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
            startDestination = "home",
            modifier = Modifier.fillMaxSize(),
        ) {
            composable(route = "home") {
                toolbarTitle = stringResource(id = R.string.debug_drawer_title)
                backButtonVisible = false

                DebugDrawerHome(menuItems = homeMenuItems)
            }

            homeMenuItems.forEachIndexed { index, item ->
                composable(route = "screen_$index") {
                    toolbarTitle = item.label
                    backButtonVisible = true
                    Text(
                        text = "Screen $index",
                        color = FirefoxTheme.colors.textPrimary,
                        style = FirefoxTheme.typography.headline6,
                    )
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

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            DebugDrawer(
                navController = navController,
                onBackButtonClick = {
                    navController.popBackStack()
                },
            )
        }
    }
}
