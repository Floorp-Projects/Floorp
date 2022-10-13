/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.app.ActivityManager.TaskDescription
import android.graphics.Bitmap
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.core.net.toUri
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifest.Icon.Purpose
import mozilla.components.support.utils.ColorUtils.isDark

private const val MIN_INSTALLABLE_ICON_SIZE = 192

/**
 * Checks if the web app manifest can be used to create a shortcut icon.
 *
 * Websites have an installable icon if the manifest contains an icon of at least 192x192.
 * @see [installableManifest]
 */
fun WebAppManifest.hasLargeIcons() = icons.any { icon ->
    (Purpose.ANY in icon.purpose || Purpose.MASKABLE in icon.purpose) &&
        icon.sizes.any { size ->
            size.minLength >= MIN_INSTALLABLE_ICON_SIZE
        }
}

/**
 * Creates a [TaskDescription] for the activity manager based on the manifest.
 *
 * Since the web app icon is provided dynamically by the web site, we can't provide a resource ID.
 * Instead we use the deprecated constructor.
 */
@Suppress("Deprecation")
fun WebAppManifest.toTaskDescription(icon: Bitmap?) =
    TaskDescription(name, icon, themeColor ?: 0)

/**
 * Creates a [CustomTabConfig] that styles a custom tab toolbar to match the manifest theme.
 */
fun WebAppManifest.toCustomTabConfig(): CustomTabConfig {
    val backgroundColor = this.backgroundColor
    return CustomTabConfig(
        toolbarColor = themeColor,
        navigationBarColor = if (SDK_INT >= Build.VERSION_CODES.O && backgroundColor != null) {
            if (isDark(backgroundColor)) Color.BLACK else Color.WHITE
        } else {
            null
        },
        closeButtonIcon = null,
        enableUrlbarHiding = true,
        actionButtonConfig = null,
        showCloseButton = false,
        showShareMenuItem = true,
        menuItems = emptyList(),
        externalAppType = ExternalAppType.PROGRESSIVE_WEB_APP,
    )
}

/**
 * Returns the scope of the manifest as a [Uri] for use
 * with [mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature].
 *
 * Null is returned when the scope should be ignored, such as with display: minimal-ui,
 * where the toolbar should always be displayed.
 */
fun WebAppManifest.getTrustedScope(): Uri? {
    return when (display) {
        WebAppManifest.DisplayMode.FULLSCREEN,
        WebAppManifest.DisplayMode.STANDALONE,
        -> (scope ?: startUrl).toUri()

        WebAppManifest.DisplayMode.MINIMAL_UI,
        WebAppManifest.DisplayMode.BROWSER,
        -> null
    }
}
