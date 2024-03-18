/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webextension

import mozilla.components.concept.engine.webextension.WebExtensionException
import mozilla.components.concept.engine.webextension.WebExtensionInstallException
import org.mozilla.geckoview.WebExtension.InstallException
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_BLOCKLISTED
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_CORRUPT_FILE
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_INCOMPATIBLE
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_NETWORK_FAILURE
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_SIGNEDSTATE_REQUIRED
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_UNSUPPORTED_ADDON_TYPE
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_USER_CANCELED

/**
 * An unexpected gecko exception that occurs when trying to perform an action on the extension like
 * (but not exclusively) installing/uninstalling, removing or updating..
 */
class GeckoWebExtensionException(throwable: Throwable) : WebExtensionException(throwable) {
    override val isRecoverable: Boolean = throwable is InstallException &&
        throwable.code == ERROR_USER_CANCELED

    companion object {
        internal fun createWebExtensionException(throwable: Throwable): WebExtensionException {
            if (throwable is InstallException) {
                return when (throwable.code) {
                    ERROR_USER_CANCELED -> WebExtensionInstallException.UserCancelled(
                        extensionName = throwable.extensionName,
                        throwable,
                    )

                    ERROR_BLOCKLISTED -> WebExtensionInstallException.Blocklisted(
                        extensionName = throwable.extensionName,
                        throwable,
                    )

                    ERROR_CORRUPT_FILE -> WebExtensionInstallException.CorruptFile(
                        throwable = throwable,
                    )

                    ERROR_NETWORK_FAILURE -> WebExtensionInstallException.NetworkFailure(
                        throwable = throwable,
                    )

                    ERROR_SIGNEDSTATE_REQUIRED -> WebExtensionInstallException.NotSigned(
                        throwable = throwable,
                    )

                    ERROR_INCOMPATIBLE -> WebExtensionInstallException.Incompatible(
                        extensionName = throwable.extensionName,
                        throwable,
                    )

                    ERROR_UNSUPPORTED_ADDON_TYPE -> WebExtensionInstallException.UnsupportedAddonType(
                        extensionName = throwable.extensionName,
                        throwable,
                    )

                    else -> WebExtensionInstallException.Unknown(
                        extensionName = throwable.extensionName,
                        throwable,
                    )
                }
            }

            return GeckoWebExtensionException(throwable)
        }
    }
}
