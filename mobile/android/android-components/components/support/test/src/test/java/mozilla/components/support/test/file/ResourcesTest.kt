/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.robolectric.mozilla.components.support.test.file

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.file.loadResourceAsString
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ResourcesTest {

    @Test
    fun getProvidedAppContext() {
        assertEquals("42", loadResourceAsString("/example_file.txt"))
    }
}
