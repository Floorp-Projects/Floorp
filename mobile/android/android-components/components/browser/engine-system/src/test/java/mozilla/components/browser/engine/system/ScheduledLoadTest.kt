package mozilla.components.browser.engine.system

import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class ScheduledLoadTest {
    @Test
    fun testConstructorNullability() {
        var scheduledLoad = ScheduledLoad()
        assertNull(scheduledLoad.url)
        assertNull(scheduledLoad.data)
        assertNull(scheduledLoad.mimeType)

        scheduledLoad = ScheduledLoad("https://mozilla.org")
        assertNotNull(scheduledLoad.url)
        assertNull(scheduledLoad.data)
        assertNull(scheduledLoad.mimeType)

        scheduledLoad = ScheduledLoad("<html><body></body></html>", "text/html")
        assertNull(scheduledLoad.url)
        assertNotNull(scheduledLoad.data)
        assertNotNull(scheduledLoad.mimeType)
    }
}