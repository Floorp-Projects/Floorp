package mozilla.components.browser.engine.gecko.integration

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.runner.RunWith
import org.junit.Test
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
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
    var nextValue: String
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
