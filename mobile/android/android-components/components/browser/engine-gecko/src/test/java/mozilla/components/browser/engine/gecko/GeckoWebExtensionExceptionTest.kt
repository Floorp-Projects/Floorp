/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.webextension.GeckoWebExtensionException
import mozilla.components.concept.engine.webextension.WebExtensionInstallException
import mozilla.components.support.test.mock
import mozilla.components.test.ReflectionUtils
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.WebExtension
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_BLOCKLISTED
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_CORRUPT_FILE
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_INCOMPATIBLE
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_NETWORK_FAILURE
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_SIGNEDSTATE_REQUIRED
import org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_USER_CANCELED

@RunWith(AndroidJUnit4::class)
class GeckoWebExtensionExceptionTest {

    @Test
    fun `Handles an user cancelled exception`() {
        val geckoException = mock<WebExtension.InstallException>()
        ReflectionUtils.setField(geckoException, "code", ERROR_USER_CANCELED)
        val webExtensionException =
            GeckoWebExtensionException.createWebExtensionException(geckoException)

        assertTrue(webExtensionException is WebExtensionInstallException.UserCancelled)
    }

    @Test
    fun `Handles a generic exception`() {
        val geckoException = Exception()
        val webExtensionException =
            GeckoWebExtensionException.createWebExtensionException(geckoException)

        assertTrue(webExtensionException is GeckoWebExtensionException)
    }

    @Test
    fun `Handles a blocklisted exception`() {
        val geckoException = mock<WebExtension.InstallException>()
        ReflectionUtils.setField(geckoException, "code", ERROR_BLOCKLISTED)
        val webExtensionException =
            GeckoWebExtensionException.createWebExtensionException(geckoException)

        assertTrue(webExtensionException is WebExtensionInstallException.Blocklisted)
    }

    @Test
    fun `Handles a CorruptFile exception`() {
        val geckoException = mock<WebExtension.InstallException>()
        ReflectionUtils.setField(geckoException, "code", ERROR_CORRUPT_FILE)
        val webExtensionException =
            GeckoWebExtensionException.createWebExtensionException(geckoException)

        assertTrue(webExtensionException is WebExtensionInstallException.CorruptFile)
    }

    @Test
    fun `Handles a NetworkFailure exception`() {
        val geckoException = mock<WebExtension.InstallException>()
        ReflectionUtils.setField(geckoException, "code", ERROR_NETWORK_FAILURE)
        val webExtensionException =
            GeckoWebExtensionException.createWebExtensionException(geckoException)

        assertTrue(webExtensionException is WebExtensionInstallException.NetworkFailure)
    }

    @Test
    fun `Handles an NotSigned exception`() {
        val geckoException = mock<WebExtension.InstallException>()
        ReflectionUtils.setField(
            geckoException,
            "code",
            ERROR_SIGNEDSTATE_REQUIRED,
        )
        val webExtensionException =
            GeckoWebExtensionException.createWebExtensionException(geckoException)

        assertTrue(webExtensionException is WebExtensionInstallException.NotSigned)
    }

    @Test
    fun `Handles an Incompatible exception`() {
        val geckoException = mock<WebExtension.InstallException>()
        ReflectionUtils.setField(
            geckoException,
            "code",
            ERROR_INCOMPATIBLE,
        )
        val webExtensionException =
            GeckoWebExtensionException.createWebExtensionException(geckoException)

        assertTrue(webExtensionException is WebExtensionInstallException.Incompatible)
    }
}
