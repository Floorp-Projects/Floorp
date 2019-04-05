/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.pipeline

import mozilla.components.browser.icons.IconRequest
import kotlin.math.min

/**
 * This [Comparator] implementations compares [IconRequest.Resource] objects to determine which icon to try to load
 * first.
 */
internal object IconResourceComparator : Comparator<IconRequest.Resource> {
    @Suppress("ComplexMethod", "ReturnCount")
    override fun compare(resource: IconRequest.Resource, other: IconRequest.Resource): Int {
        if (resource.url == other.url) {
            // Two resources pointing to the same URL are always referencing the same icon. So treat them as equal.
            return 0
        }

        if (resource.type != other.type) {
            return if (resource.type.rank() > other.type.rank()) -1 else 1
        }

        if (resource.maxSize != other.maxSize) {
            return if (resource.maxSize > other.maxSize) -1 else 1
        }

        // If there's no other way to choose, we prefer container types. They *might* contain an image larger than the
        // size given in the <link> tag.
        val isResourceContainerType = resource.isContainerType
        val isOtherContainerType = other.isContainerType
        if (isResourceContainerType != isOtherContainerType) {
            return if (isResourceContainerType) -1 else 1
        }

        // There's no way to know which icon might be better. However we need to pick a consistent one to avoid breaking
        // set implementations (See Fennec Bug 1331808). Therefore we are  picking one by just comparing the URLs.
        return resource.url.compareTo(other.url)
    }
}

@Suppress("MagicNumber")
private fun IconRequest.Resource.Type.rank(): Int {
    return when (this) {
        // We prefer touch icons because they tend to have a higher resolution than ordinary favicons.
        IconRequest.Resource.Type.APPLE_TOUCH_ICON -> 15
        IconRequest.Resource.Type.FAVICON -> 10

        // Fallback icon types:
        IconRequest.Resource.Type.IMAGE_SRC -> 6
        IconRequest.Resource.Type.FLUID_ICON -> 5
        IconRequest.Resource.Type.OPENGRAPH -> 4
        IconRequest.Resource.Type.TWITTER -> 3
        IconRequest.Resource.Type.MICROSOFT_TILE -> 2
    }
}

private val IconRequest.Resource.maxSize: Int
    get() {
        return sizes.maxBy { size ->
            min(size.width, size.height)
        }?.let { min(it.width, it.height) } ?: 0
    }

private val IconRequest.Resource.isContainerType: Boolean
    get() = mimeType != null && containerTypes.contains(mimeType)

private val containerTypes = listOf(
    "image/vnd.microsoft.icon",
    "image/ico",
    "image/icon",
    "image/x-icon",
    "text/ico",
    "application/ico"
)
