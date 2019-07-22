/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

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
    resources = icons.mapNotNull { it.toIconResource() },
    color = backgroundColor
)

private fun WebAppManifest.Icon.toIconResource(): IconRequest.Resource? {
    if (Purpose.MASKABLE !in purpose && Purpose.ANY !in purpose) return null

    return IconRequest.Resource(
        url = src,
        type = MANIFEST_ICON,
        sizes = sizes,
        mimeType = type,
        maskable = Purpose.MASKABLE in purpose
    )
}
