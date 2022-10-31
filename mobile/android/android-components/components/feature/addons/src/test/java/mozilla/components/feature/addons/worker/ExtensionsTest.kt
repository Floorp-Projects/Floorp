/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.worker

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import kotlinx.coroutines.CancellationException
import mozilla.components.concept.engine.webextension.WebExtensionException
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class ExtensionsTest {

    @Test
    fun `shouldReport - when cause is an IOException must NOT be reported`() {
        assertFalse(Exception(IOException()).shouldReport())
    }

    @Test
    fun `shouldReport - when cause is a CancellationException must NOT be reported`() {
        assertFalse(Exception(CancellationException()).shouldReport())
    }

    @Test
    fun `shouldReport - when cause the exception is a CancellationException must NOT be reported`() {
        assertFalse(CancellationException().shouldReport())
    }

    @Test
    fun `shouldReport - when the exception isRecoverable must be reported`() {
        assertTrue(WebExtensionException(java.lang.Exception(), isRecoverable = true).shouldReport())
    }

    @Test
    fun `shouldReport - when the exception NOT isRecoverable must NOT be reported`() {
        assertFalse(WebExtensionException(java.lang.Exception(), isRecoverable = false).shouldReport())
    }
}
