/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks

/**
 * Uniquely identifies an asset.
 *
 * A digital asset is an identifiable and addressable online entity that typically provides some
 * service or content.
 */
sealed class AssetDescriptor {

    /**
     * A web site asset descriptor.
     * @property site URI representing the domain of the website.
     * @sample
     * AssetDescriptor.Web(
     *   site = "https://{fully-qualified domain}{:optional port}"
     * )
     */
    data class Web(
        val site: String,
    ) : AssetDescriptor()

    /**
     * An Android app asset descriptor.
     * @property packageName Package name for the Android app.
     * @property sha256CertFingerprint A colon-separated hex string.
     * @sample
     * AssetDescriptor.Android(
     *   packageName = "com.costingtons.app",
     *   sha256CertFingerprint = "A0:83:42:E6:1D:BE:A8:8A:04:96:B2:3F:CF:44:E5"
     * )
     */
    data class Android(
        val packageName: String,
        val sha256CertFingerprint: String,
    ) : AssetDescriptor()
}
