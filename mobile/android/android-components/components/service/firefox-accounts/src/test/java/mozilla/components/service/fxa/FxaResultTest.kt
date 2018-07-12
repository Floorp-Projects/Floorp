package mozilla.components.service.fxa

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FxaResultTest {

    @Test
    fun thenWithResult() {
        var chainComplete = false

        FxaResult.fromValue(42).then { value: Int ->
            assertEquals(value, 42)
            chainComplete = true
            FxaResult<Void>()
        }

        assertTrue(chainComplete)
    }

    @Test
    fun thenWithException() {
        var chainComplete = false

        FxaResult.fromException<Void>(Exception("exception message")).then({ value: Void ->
            assertNull("valueListener should not be called", value)
            FxaResult<Void>()
        }, { value: Exception ->
            assertEquals(value.message, "exception message")
            chainComplete = true
            FxaResult()
        })

        assertTrue(chainComplete)
    }

    @Test
    fun thenWithThrownException() {
        var chainComplete = false

        FxaResult.fromValue(42).then { _: Int ->
            throw Exception("exception message")
            FxaResult<Void>()
        }.then({
            fail("valueListener should not be called")
            FxaResult<Void>()
        }, { value: Exception ->
            assertEquals(value.message, "exception message")
            chainComplete = true
            FxaResult()
        })

        assertTrue(chainComplete)
    }

    @Test
    fun resultChaining() {
        var chainComplete = false

        FxaResult.fromValue(42).then { value: Int ->
            assertEquals(value, 42)
            FxaResult.fromValue("string")
        }.then { value: String ->
            assertEquals(value, "string")
            FxaResult.fromException<Void>(Exception("exception message"))
        }.then({
            fail("valueListener should not be called")
            FxaResult<Void>()
        }, { value: Exception ->
            assertEquals(value.message, "exception message")
            chainComplete = true
            FxaResult()
        })

        assertTrue(chainComplete)
    }

    @Test
    fun whenComplete() {
        var chainComplete = false

        FxaResult.fromValue(42).then { value: Int ->
            assertEquals(value, 42)
            FxaResult.fromValue("string")
        }.whenComplete { value: String ->
            assertEquals(value, "string")
            chainComplete = true
        }

        assertTrue(chainComplete)
    }
}
