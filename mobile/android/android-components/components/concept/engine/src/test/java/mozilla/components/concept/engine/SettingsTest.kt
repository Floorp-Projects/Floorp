/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import org.junit.Assert.fail
import org.junit.Test

class SettingsTest {

    @Test
    fun testSettingsThrowByDefault() {
        val settings = object : Settings { }
        expectUnsupportedSettingException { settings.javascriptEnabled }
        expectUnsupportedSettingException { settings.javascriptEnabled = false }
        expectUnsupportedSettingException { settings.domStorageEnabled }
        expectUnsupportedSettingException { settings.domStorageEnabled = false }
    }

    private fun expectUnsupportedSettingException(f: () -> Unit) {
        try {
            f()
            fail("Expected UnsupportedSettingException")
        } catch (e: UnsupportedSettingException) { }
    }
}