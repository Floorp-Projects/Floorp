/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.material.MaterialTheme
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import org.mozilla.samples.compose.browser.browser.BrowserScreen
import org.mozilla.samples.compose.browser.settings.SettingsScreen

/**
 * Ladies and gentleman, the browser. ¯\_(ツ)_/¯
 */
class BrowserComposeActivity : AppCompatActivity() {
    companion object {
        const val ROUTE_BROWSER = "browser"
        const val ROUTE_SETTINGS = "settings"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            val navController = rememberNavController()

            MaterialTheme {
                NavHost(navController, startDestination = ROUTE_BROWSER) {
                    composable(ROUTE_BROWSER) { BrowserScreen(navController) }
                    composable(ROUTE_SETTINGS) { SettingsScreen() }
                }
            }
        }
    }
}
