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
    fun thenWithResultAsync() {
        var chainComplete = false
        val result = FxaResult<Int>()

        result.then { value: Int ->
            assertEquals(value, 42)
            chainComplete = true
            FxaResult<Void>()
        }

        result.complete(42)

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
    fun thenWithExceptionAsync() {
        var chainComplete = false

        val result = FxaResult<Void>()

        result.then({ value: Void ->
            assertNull("valueListener should not be called", value)
            FxaResult<Void>()
        }, { value: Exception ->
            assertEquals(value.message, "exception message")
            chainComplete = true
            FxaResult()
        })

        result.completeExceptionally(Exception("exception message"))

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
    fun thenWithThrownExceptionAsync() {
        var chainComplete = false

        val result = FxaResult<Int>()

        result.then { _: Int ->
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

        result.complete(42)

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
    fun resultChainingAsyncInitial() {
        var chainComplete = false

        val result = FxaResult<Int>()

        result.then { value: Int ->
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

        result.complete(42)

        assertTrue(chainComplete)
    }

    @Test
    fun resultChainingAsyncInOrder() {
        var chainComplete = false

        val resultInt = FxaResult<Int>()
        val resultString = FxaResult<String>()
        val resultError = FxaResult<Void>()

        resultInt.then { value: Int ->
            assertEquals(value, 42)
            resultString
        }.then { value: String ->
            assertEquals(value, "string")
            resultError
        }.then({
            fail("valueListener should not be called")
            FxaResult<Void>()
        }, { value: Exception ->
            assertEquals(value.message, "exception message")
            chainComplete = true
            FxaResult()
        })

        resultInt.complete(42)

        resultString.complete("string")

        resultError.completeExceptionally(Exception("exception message"))

        assertTrue(chainComplete)
    }

    @Test
    fun resultChainingAsyncOutOfOrder() {
        var chainComplete = false

        val resultInt = FxaResult<Int>()
        val resultString = FxaResult<String>()
        val resultError = FxaResult<Void>()

        resultInt.then { value: Int ->
            assertEquals(value, 42)
            resultString
        }.then { value: String ->
            assertEquals(value, "string")
            resultError
        }.then({
            fail("valueListener should not be called")
            FxaResult<Void>()
        }, { value: Exception ->
            assertEquals(value.message, "exception message")
            chainComplete = true
            FxaResult()
        })

        resultError.completeExceptionally(Exception("exception message"))

        resultString.complete("string")

        resultInt.complete(42)

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

    @Test
    fun whenCompleteAsync() {
        var chainComplete = false
        val result = FxaResult<Int>()
        result.then { value: Int ->
            assertEquals(value, 42)
            FxaResult.fromValue("string")
        }.whenComplete { value: String ->
            assertEquals(value, "string")
            chainComplete = true
        }
        result.complete(42)

        assertTrue(chainComplete)
    }

    @Test
    fun nullableResult() {
        var chainComplete = false

        val nullableAndNull: FxaResult<Int?> = FxaResult.fromValue(null)

        nullableAndNull.then { value ->
            assertEquals(value, null)
            val nullableButExists: FxaResult<Int?> = FxaResult.fromValue(4)
            nullableButExists
        }.whenComplete { value ->
            assertEquals(value, 4)
            chainComplete = true
        }

        assertTrue(chainComplete)
    }

    @Test
    fun nullableResultAsync() {
        var chainComplete = false

        val nullableAndNull: FxaResult<Int?> = FxaResult()

        nullableAndNull.then { value ->
            assertEquals(value, null)
            val nullableButExists: FxaResult<Int?> = FxaResult.fromValue(4)
            nullableButExists
        }.whenComplete { value ->
            assertEquals(value, 4)
            chainComplete = true
        }

        nullableAndNull.complete(null)

        assertTrue(chainComplete)
    }

    @Test
    fun exceptionChainSkipsMissing() {
        var chainComplete = false

        FxaResult.fromValue(42).then { value: Int ->
            assertEquals(value, 42)
            FxaResult.fromException<String>(Exception("test"))
        }.then { value: String ->
            fail("Shouldn't call onValue (1)")
            FxaResult.fromValue("dummy 1")
        }.then({ value: String ->
            fail("Shouldn't call onValue (2)")
            FxaResult.fromValue("dummy 2")
        }, { value: Exception ->
            assertEquals(value.message, "test")
            chainComplete = true
            FxaResult()
        })

        assertTrue(chainComplete)
    }

    @Test
    fun exceptionChainSkipsMissingAsync() {
        var chainComplete = false
        val resultInt = FxaResult<Int>()
        val resultErr = FxaResult<String>()

        resultInt.then { value: Int ->
            assertEquals(value, 42)
            resultErr
        }.then { value: String ->
            fail("Shouldn't call onValue (1)")
            FxaResult.fromValue("dummy 1")
        }.then({ value: String ->
            fail("Shouldn't call onValue (2)")
            FxaResult.fromValue("dummy 2")
        }, { value: Exception ->
            assertEquals(value.message, "test")
            chainComplete = true
            FxaResult()
        })

        resultInt.complete(42)
        resultErr.completeExceptionally(Exception("test"))

        assertTrue(chainComplete)
    }

    @Test
    fun exceptionChainHandlesThrows() {
        var chainComplete = false

        FxaResult.fromException<Int>(Exception("test 1")).then({ value: Int ->
            fail("Shouldn't call onValue (1)")
            FxaResult.fromValue("dummy value 0")
        }, { error ->
            assertEquals(error.message, "test 1")
            throw Exception("test 2")
        }).then({
            fail("Shouldn't call onValue (2)")
            FxaResult.fromValue("dummy value 1")
        }, { error ->
            assertEquals(error.message, "test 2")
            FxaResult.fromValue("value")
        }).whenComplete { value ->
            assertEquals(value, "value")
            chainComplete = true
        }

        assertTrue(chainComplete)
    }

    @Test
    fun exceptionChainHandlesThrowsAsync() {
        var chainComplete = false
        val resultErr = FxaResult<Int>()
        resultErr.then({ value: Int ->
            fail("Shouldn't call onValue (1)")
            FxaResult.fromValue("dummy value 0")
        }, { error ->
            assertEquals(error.message, "test 1")
            throw Exception("test 2")
        }).then({
            fail("Shouldn't call onValue (2)")
            FxaResult.fromValue("dummy value 1")
        }, { error ->
            assertEquals(error.message, "test 2")
            FxaResult.fromValue("value")
        }).whenComplete { value ->
            assertEquals(value, "value")
            chainComplete = true
        }

        resultErr.completeExceptionally(Exception("test 1"))

        assertTrue(chainComplete)
    }
}
