/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.os.Build
import android.provider.Settings
import androidx.annotation.RequiresApi
import androidx.core.os.bundleOf
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.ManufacturerCodes

const val SETTINGS_SELECT_OPTION_KEY = ":settings:fragment_args_key"
const val SETTINGS_SHOW_FRAGMENT_ARGS = ":settings:show_fragment_args"
const val DEFAULT_BROWSER_APP_OPTION = "default_browser"
const val ACTION_MANAGE_DEFAULT_APPS_SETTINGS_HUAWEI = "com.android.settings.PREFERRED_SETTINGS"
private val logger = Logger("navigateToDefaultBrowserAppsSettings")

/**
 * Open OS settings for default browser.
 */
@RequiresApi(Build.VERSION_CODES.N)
fun Context.navigateToDefaultBrowserAppsSettings() {
    val intent = when {
        ManufacturerCodes.isHuawei -> Intent(ACTION_MANAGE_DEFAULT_APPS_SETTINGS_HUAWEI)
        else -> Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS).apply {
            putExtra(
                SETTINGS_SELECT_OPTION_KEY,
                DEFAULT_BROWSER_APP_OPTION,
            )
            putExtra(
                SETTINGS_SHOW_FRAGMENT_ARGS,
                bundleOf(SETTINGS_SELECT_OPTION_KEY to DEFAULT_BROWSER_APP_OPTION),
            )
        }
    }

    try {
        startActivity(intent)
    } catch (e: ActivityNotFoundException) {
        logger.error("ActivityNotFoundException " + e.message.toString())
    }
}
