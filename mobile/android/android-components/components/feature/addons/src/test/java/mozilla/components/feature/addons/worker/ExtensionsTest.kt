/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.worker

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertFalse
import java.io.IOException
import kotlinx.coroutines.CancellationException
import org.junit.Test
import org.junit.runner.RunWith

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
}
