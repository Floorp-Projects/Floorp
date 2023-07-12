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
        val geckoException = mock<WebExtension.InstallException>()
        val webExtensionException =
            GeckoWebExtensionException.createWebExtensionException(geckoException)

        assertTrue(webExtensionException is GeckoWebExtensionException)
    }
}
