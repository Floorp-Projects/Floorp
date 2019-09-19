/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.verify

import android.content.pm.PackageManager
import android.content.pm.Signature
import android.net.Uri
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.annotation.VisibleForTesting
import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsService.RELATION_USE_AS_ORIGIN
import androidx.browser.customtabs.CustomTabsService.Relation
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.withContext
import mozilla.components.concept.fetch.Client
import java.io.ByteArrayInputStream
import java.security.MessageDigest
import java.security.NoSuchAlgorithmException
import java.security.cert.CertificateEncodingException
import java.security.cert.CertificateException
import java.security.cert.CertificateFactory
import java.security.cert.X509Certificate

/**
 * Used to verify postMessage origin for a designated package name.
 *
 * Uses Digital Asset Links to confirm that the given origin is associated with the package name.
 * It caches any origin that has been verified during the current application
 * lifecycle and reuses that without making any new network requests.
 */
class OriginVerifier(
    private val packageName: String,
    @Relation private val relation: Int,
    packageManager: PackageManager,
    httpClient: Client,
    apiKey: String?
) {

    @VisibleForTesting
    internal val handler = DigitalAssetLinksHandler(httpClient, apiKey)
    @VisibleForTesting
    internal val signatureFingerprint by lazy {
        getCertificateSHA256FingerprintForPackage(packageManager)
    }

    /**
     * Verify the claimed origin for the cached package name asynchronously. This will end up
     * making a network request for non-cached origins with a HTTP [Client].
     *
     * @param origin The postMessage origin the application is claiming to have. Can't be null.
     */
    suspend fun verifyOrigin(origin: Uri) = withContext(IO) { verifyOriginInternal(origin) }

    @Suppress("ReturnCount")
    private fun verifyOriginInternal(origin: Uri): Boolean {
        val cachedOrigin = cachedOriginMap[packageName]
        if (cachedOrigin == origin) return true

        if (origin.scheme != "https") return false
        val relationship = when (relation) {
            RELATION_USE_AS_ORIGIN -> USE_AS_ORIGIN
            RELATION_HANDLE_ALL_URLS -> HANDLE_ALL_URLS
            else -> return false
        }

        val originVerified = handler.checkDigitalAssetLinkRelationship(
            origin.toString(),
            packageName,
            signatureFingerprint,
            relationship = relationship
        )

        if (originVerified && packageName !in cachedOriginMap) {
            cachedOriginMap[packageName] = origin
        }
        return originVerified
    }

    /**
    * Computes the SHA256 certificate for the given package name. The app with the given package
    * name has to be installed on device. The output will be a 30 long HEX string with : between
    * each value.
    * @return The SHA256 certificate for the package name.
    */
    @VisibleForTesting
    internal fun getCertificateSHA256FingerprintForPackage(packageManager: PackageManager): String? {
        val signature = packageManager.getSignature(packageName) ?: return null

        val input = ByteArrayInputStream(signature.toByteArray())
        return try {
            val certificate = CertificateFactory.getInstance("X509").generateCertificate(input) as X509Certificate
            byteArrayToHexString(MessageDigest.getInstance("SHA256").digest(certificate.encoded))
        } catch (e: CertificateEncodingException) {
            // Certificate type X509 encoding failed
            null
        } catch (e: CertificateException) {
            throw AssertionError("Should not happen", e)
        } catch (e: NoSuchAlgorithmException) {
            throw AssertionError("Should not happen", e)
        }
    }

    companion object {
        private val HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray()
        private const val USE_AS_ORIGIN = "delegate_permission/common.use_as_origin"
        private const val HANDLE_ALL_URLS = "delegate_permission/common.handle_all_urls"

        private val cachedOriginMap = mutableMapOf<String, Uri>()

        @Suppress("PackageManagerGetSignatures", "Deprecation")
        // https://stackoverflow.com/questions/39192844/android-studio-warning-when-using-packagemanager-get-signatures
        private fun PackageManager.getSignature(packageName: String): Signature? {
            val packageInfo = try {
                if (SDK_INT >= Build.VERSION_CODES.P) {
                    getPackageInfo(packageName, PackageManager.GET_SIGNING_CERTIFICATES)
                } else {
                    getPackageInfo(packageName, PackageManager.GET_SIGNATURES)
                }
            } catch (e: PackageManager.NameNotFoundException) {
                // Will return null if there is no package found.
                return null
            }

            return if (SDK_INT >= Build.VERSION_CODES.P) {
                val signingInfo = packageInfo.signingInfo
                if (signingInfo.hasMultipleSigners()) {
                    signingInfo.apkContentsSigners.firstOrNull()
                } else {
                    signingInfo.signingCertificateHistory.firstOrNull()
                }
            } else {
                packageInfo.signatures.first()
            }
        }

        /**
         * Converts a byte array to hex string with : inserted between each element.
         * @param byteArray The array to be converted.
         * @return A string with two letters representing each byte and : in between.
         */
        @Suppress("MagicNumber")
        @VisibleForTesting
        internal fun byteArrayToHexString(bytes: ByteArray): String {
            val hexString = StringBuilder(bytes.size * 3 - 1)
            var v: Int
            for (j in bytes.indices) {
                v = bytes[j].toInt() and 0xFF
                hexString.append(HEX_CHAR_LOOKUP[v.ushr(4)])
                hexString.append(HEX_CHAR_LOOKUP[v and 0x0F])
                if (j < bytes.lastIndex) hexString.append(':')
            }
            return hexString.toString()
        }
    }
}
