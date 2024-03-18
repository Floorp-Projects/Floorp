/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import android.graphics.Color
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.IconRequest.Resource.Type.MANIFEST_ICON
import mozilla.components.browser.icons.IconRequest.Size.LAUNCHER
import mozilla.components.browser.icons.IconRequest.Size.LAUNCHER_ADAPTIVE
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifest.Icon.Purpose

/**
 * Creates an [IconRequest] for retrieving the icon specified in the manifest.
 */
fun WebAppManifest.toIconRequest() = IconRequest(
    url = startUrl,
    size = if (SDK_INT >= Build.VERSION_CODES.O) LAUNCHER_ADAPTIVE else LAUNCHER,
    resources = icons
        .filter { Purpose.MASKABLE in it.purpose || Purpose.ANY in it.purpose }
        .map { it.toIconResource() },
    color = backgroundColor,
)

/**
 * Creates an [IconRequest] for retrieving a monochrome icon specified in the manifest.
 */
fun WebAppManifest.toMonochromeIconRequest() = IconRequest(
    url = startUrl,
    size = IconRequest.Size.DEFAULT,
    resources = icons
        .filter { Purpose.MONOCHROME in it.purpose }
        .map { it.toIconResource() },
    color = Color.WHITE,
)

private fun WebAppManifest.Icon.toIconResource(): IconRequest.Resource {
    return IconRequest.Resource(
        url = src,
        type = MANIFEST_ICON,
        sizes = sizes,
        mimeType = type,
        maskable = Purpose.MASKABLE in purpose,
    )
}
