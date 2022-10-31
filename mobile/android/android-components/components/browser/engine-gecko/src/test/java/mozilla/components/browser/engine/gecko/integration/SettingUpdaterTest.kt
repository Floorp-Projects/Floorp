/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.integration

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SettingUpdaterTest {

    @Test
    fun `test updateValue`() {
        val subject = DummySettingUpdater("current", "new")
        assertEquals("current", subject.value)

        subject.updateValue()
        assertEquals("new", subject.value)
    }

    @Test
    fun `test enabled updates value`() {
        val subject = DummySettingUpdater("current", "new")
        assertEquals("current", subject.value)

        subject.enabled = true
        assertEquals("new", subject.value)

        // disabling doesn't update the value.
        subject.nextValue = "disabled"
        subject.enabled = false
        assertEquals("new", subject.value)
    }

    @Test
    fun `test registering and deregistering for updates`() {
        val subject = DummySettingUpdater("current", "new")
        assertFalse("Initialized not registering for updates", subject.registered)

        subject.updateValue()
        assertFalse("updateValue not registering for updates", subject.registered)

        subject.enabled = true
        assertTrue("enabled = true registering for updates", subject.registered)

        subject.enabled = false
        assertFalse("enabled = false deregistering for updates", subject.registered)
    }
}

class DummySettingUpdater(
    override var value: String = "",
    var nextValue: String,
) : SettingUpdater<String>() {

    var registered = false

    override fun registerForUpdates() {
        registered = true
    }

    override fun unregisterForUpdates() {
        registered = false
    }

    override fun findValue() = nextValue
}
