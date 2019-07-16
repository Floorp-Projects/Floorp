/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.app.ActivityManager.TaskDescription
import android.graphics.Bitmap
import android.graphics.Color
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.utils.ColorUtils.isDark

/**
 * Create a [TaskDescription] for the activity manager based on the manifest.
 *
 * Since the web app icon is provided dynamically by the web site, we can't provide a resource ID.
 * Instead we use the deprecated constructor.
 */
@Suppress("Deprecation")
fun WebAppManifest.toTaskDescription(icon: Bitmap?) =
    TaskDescription(name, icon, themeColor ?: 0)

fun WebAppManifest.toCustomTabConfig() =
    CustomTabConfig(
        id = startUrl,
        toolbarColor = themeColor,
        navigationBarColor = backgroundColor?.let {
            if (isDark(it)) Color.BLACK else Color.WHITE
        },
        closeButtonIcon = null,
        enableUrlbarHiding = true,
        actionButtonConfig = null,
        showShareMenuItem = true,
        menuItems = emptyList()
    )
