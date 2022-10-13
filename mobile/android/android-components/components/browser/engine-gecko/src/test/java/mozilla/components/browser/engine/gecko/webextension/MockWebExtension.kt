/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webextension

import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.test.ReflectionUtils
import org.mockito.Mockito.doNothing
import org.mozilla.geckoview.Image
import org.mozilla.geckoview.WebExtension

fun mockNativeWebExtension(
    id: String = "id",
    location: String = "uri",
    flags: Int = 0,
    isBuiltIn: Boolean = false,
    metaData: WebExtension.MetaData? = null,
): WebExtension {
    val extension: WebExtension = mock()
    ReflectionUtils.setField(extension, "id", id)
    ReflectionUtils.setField(extension, "location", location)
    ReflectionUtils.setField(extension, "flags", flags)
    ReflectionUtils.setField(extension, "isBuiltIn", isBuiltIn)
    ReflectionUtils.setField(extension, "metaData", metaData)

    doNothing().`when`(extension).setActionDelegate(any())
    return extension
}

fun mockNativeWebExtensionMetaData(
    icon: Image = mock(),
    permissions: Array<String> = emptyArray(),
    origins: Array<String> = emptyArray(),
    name: String? = null,
    description: String? = null,
    version: String? = null,
    creatorName: String? = null,
    creatorUrl: String? = null,
    homepageUrl: String? = null,
    optionsPageUrl: String? = null,
    openOptionsPageInTab: Boolean = false,
    isRecommended: Boolean = false,
    blocklistState: Int = 0,
    signedState: Int = 0,
    disabledFlags: Int = 0,
    baseUrl: String = "",
    allowedInPrivateBrowsing: Boolean = false,
    enabled: Boolean = false,
    temporary: Boolean = false,
): WebExtension.MetaData {
    val metadata: WebExtension.MetaData = mock()
    ReflectionUtils.setField(metadata, "icon", icon)
    ReflectionUtils.setField(metadata, "permissions", permissions)
    ReflectionUtils.setField(metadata, "origins", origins)
    ReflectionUtils.setField(metadata, "name", name)
    ReflectionUtils.setField(metadata, "description", description)
    ReflectionUtils.setField(metadata, "version", version)
    ReflectionUtils.setField(metadata, "creatorName", creatorName)
    ReflectionUtils.setField(metadata, "creatorUrl", creatorUrl)
    ReflectionUtils.setField(metadata, "homepageUrl", homepageUrl)
    ReflectionUtils.setField(metadata, "optionsPageUrl", optionsPageUrl)
    ReflectionUtils.setField(metadata, "openOptionsPageInTab", openOptionsPageInTab)
    ReflectionUtils.setField(metadata, "isRecommended", isRecommended)
    ReflectionUtils.setField(metadata, "blocklistState", blocklistState)
    ReflectionUtils.setField(metadata, "signedState", signedState)
    ReflectionUtils.setField(metadata, "disabledFlags", disabledFlags)
    ReflectionUtils.setField(metadata, "baseUrl", baseUrl)
    ReflectionUtils.setField(metadata, "allowedInPrivateBrowsing", allowedInPrivateBrowsing)
    ReflectionUtils.setField(metadata, "enabled", enabled)
    ReflectionUtils.setField(metadata, "temporary", temporary)
    return metadata
}

fun mockCreateTabDetails(
    active: Boolean,
    url: String,
): WebExtension.CreateTabDetails {
    val createTabDetails: WebExtension.CreateTabDetails = mock()
    ReflectionUtils.setField(createTabDetails, "active", active)
    ReflectionUtils.setField(createTabDetails, "url", url)
    return createTabDetails
}

fun mockUpdateTabDetails(
    active: Boolean,
): WebExtension.UpdateTabDetails {
    val updateTabDetails: WebExtension.UpdateTabDetails = mock()
    ReflectionUtils.setField(updateTabDetails, "active", active)
    return updateTabDetails
}
