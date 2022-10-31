/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.support.ktx.android.net

import android.content.Context
import androidx.core.net.toUri
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class OnDeviceUriKtTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun isUnderPrivateAppDirectory() {
        var uri = "file:///data/user/0/${context.packageName}/any_directory/file.text".toUri()

        assertTrue(uri.isUnderPrivateAppDirectory(context))

        uri = "file:///data/data/${context.packageName}/any_directory/file.text".toUri()

        assertTrue(uri.isUnderPrivateAppDirectory(context))

        uri = "file:///data/directory/${context.packageName}/any_directory/file.text".toUri()

        assertFalse(uri.isUnderPrivateAppDirectory(context))
    }
}
