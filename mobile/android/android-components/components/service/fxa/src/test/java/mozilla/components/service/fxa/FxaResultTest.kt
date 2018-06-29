package mozilla.components.service.fxa

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FxaResultTest {

    @Test
    fun thenWithResult() {
        FxaResult.fromValue(42).then { value: Int? ->
            assertEquals(value, 42)
            FxaResult<Void>()
        }
    }

    @Test
    fun thenWithException() {
        FxaResult.fromException<Void>(Exception("exception message")).then({ value: Void? ->
            assertNull("valueListener should not be called", value)
            FxaResult<Void>()
        }, { value: Exception ->
            assertEquals(value.message, "exception message")
            FxaResult()
        })
    }

    @Test
    fun resultChaining() {
        FxaResult.fromValue(42).then { value: Int? ->
            assertEquals(value, 42)
            FxaResult.fromValue("string")
        }.then { value: String? ->
            assertEquals(value, "string")
            FxaResult.fromException<Void>(Exception("exception message"))
        }.then({
            fail("valueListener should not be called")
            FxaResult<Void>()
        }, { value: Exception ->
            assertEquals(value.message, "exception message")
            FxaResult()
        })
    }
}
