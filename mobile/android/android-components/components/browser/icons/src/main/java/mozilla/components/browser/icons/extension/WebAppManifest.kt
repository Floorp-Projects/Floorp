/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifest.Icon.Purpose

fun WebAppManifest.toIconRequest() = IconRequest(
    url = startUrl,
    size = IconRequest.Size.LAUNCHER,
    resources = icons.mapNotNull { it.toIconResource() }
)

private fun WebAppManifest.Icon.toIconResource(): IconRequest.Resource? {
    if (Purpose.MASKABLE !in purpose && Purpose.ANY !in purpose) return null

    return IconRequest.Resource(
        url = src,
        type = IconRequest.Resource.Type.MANIFEST_ICON,
        sizes = sizes,
        mimeType = type,
        maskable = Purpose.MASKABLE in purpose
    )
}
