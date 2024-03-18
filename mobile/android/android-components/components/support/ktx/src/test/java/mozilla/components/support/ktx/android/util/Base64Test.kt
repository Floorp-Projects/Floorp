/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.util

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class Base64Test {

    @Test
    fun `encodeToUriString contains required data URI format`() {
        val s = Base64.encodeToUriString("foo")
        Assert.assertTrue(s.contains("data:text/html;base64,"))
        Assert.assertTrue(s.contains("Zm9v"))
    }
}
