package mozilla.components.lib.crash.sentry.eventprocessors

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.sentry.Hint
import io.sentry.SentryEvent
import io.sentry.protocol.SentryException
import junit.framework.TestCase.assertEquals
import mozilla.components.concept.base.crash.RustCrashReport
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class RustCrashEventProcessorTest {
    class TestRustException : Exception(), RustCrashReport {
        override val typeName = "test_rust_crash"
        override val message = "test_rust_message"
    }

    @Test
    fun `GIVEN a SentryEvent that contains a RustCrashReport WHEN process is called THEN a fingerprint is added and the exception type and value are cleaned up`() {
        val processor = RustCrashEventProcessor()
        val event = SentryEvent(TestRustException()).apply {
            exceptions = listOf(SentryException())
        }

        processor.process(event, Hint())
        assertEquals("test_rust_crash", event.fingerprints?.first())
        assertEquals("test_rust_crash", event.exceptions?.firstOrNull()?.type)
        assertEquals("test_rust_message", event.exceptions?.firstOrNull()?.value)
    }
}
