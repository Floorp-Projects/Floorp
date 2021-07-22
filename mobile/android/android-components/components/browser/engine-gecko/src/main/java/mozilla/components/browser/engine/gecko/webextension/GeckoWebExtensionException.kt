/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webextension

import mozilla.components.concept.engine.webextension.WebExtensionException
import org.mozilla.geckoview.WebExtension.InstallException
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_USER_CANCELED

/**
 * An unexpected gecko exception that occurs when trying to perform an action on the extension like
 * (but not exclusively) installing/uninstalling, removing or updating..
 */
class GeckoWebExtensionException(throwable: Throwable) : WebExtensionException(throwable) {
    override val isRecoverable: Boolean = throwable is InstallException &&
        throwable.code == ERROR_USER_CANCELED
}
