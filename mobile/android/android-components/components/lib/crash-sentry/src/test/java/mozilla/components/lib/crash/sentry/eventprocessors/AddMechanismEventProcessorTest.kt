package mozilla.components.lib.crash.sentry.eventprocessors

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.sentry.Hint
import io.sentry.SentryEvent
import io.sentry.SentryLevel
import io.sentry.protocol.SentryException
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertNull
import junit.framework.TestCase.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AddMechanismEventProcessorTest {
    @Test
    fun `GIVEN a FATAL SentryEvent WHEN process is called THEN a Mechanism is attached to the exception`() {
        val processor = AddMechanismEventProcessor()
        val event = SentryEvent().apply {
            level = SentryLevel.FATAL
            exceptions = listOf(SentryException())
        }

        assertNull(event.exceptions?.first()?.mechanism)
        processor.process(event, Hint())
        assertEquals(AddMechanismEventProcessor.UNCAUGHT_EXCEPTION_TYPE, event.exceptions?.first()?.mechanism?.type)
        assertTrue(event.exceptions?.first()?.mechanism?.isHandled == false)
    }

    @Test
    fun `GIVEN a less than FATAL SentryEvent WHEN process is called THEN no Mechanism is attached to the exception`() {
        val processor = AddMechanismEventProcessor()
        val event = SentryEvent().apply {
            level = SentryLevel.INFO
            exceptions = listOf(SentryException())
        }

        assertNull(event.exceptions?.first()?.mechanism)
        processor.process(event, Hint())
        assertNull(event.exceptions?.first()?.mechanism)
    }
}
