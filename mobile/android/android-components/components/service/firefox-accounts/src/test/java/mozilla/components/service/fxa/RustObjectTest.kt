/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import com.sun.jna.Memory
import com.sun.jna.PointerType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.CountDownLatch

@RunWith(RobolectricTestRunner::class)
class RustObjectTest {

    companion object {
        @BeforeClass
        @JvmStatic
        fun setup() {
            FxaClient.INSTANCE = mock(FxaClient::class.java)
        }
    }

    class MockRawPointer : PointerType()

    class MockRustObject(override var rawPointer: MockRawPointer?) : RustObject<MockRawPointer>() {
        var destroyed = false

        override fun destroy(p: MockRawPointer) {
            destroyed = true
        }
    }

    @Test(expected = NullPointerException::class)
    fun testValidPointerThrowsIfNull() {
        val rustObject = MockRustObject(null)
        rustObject.validPointer()
    }

    @Test
    fun testConsumePointer() {
        val rustObject = MockRustObject(MockRawPointer())
        assertFalse(rustObject.isConsumed)

        rustObject.consumePointer()
        assertTrue(rustObject.isConsumed)
    }

    @Test
    fun testCloseConsumesAndDestroys() {
        val rustObject = MockRustObject(MockRawPointer())
        assertFalse(rustObject.isConsumed)
        assertFalse(rustObject.destroyed)

        rustObject.close()
        assertTrue(rustObject.isConsumed)
        assertTrue(rustObject.destroyed)
    }

    @Test
    fun testGetAndConsumeString() {
        val str = "test"
        val strPointer = Memory(str.length.toLong() + 1)
        strPointer.setString(0, str)

        assertNull(RustObject.getAndConsumeString(null))
        assertEquals("test", RustObject.getAndConsumeString(strPointer))
    }

    @Test
    fun testSafeAsync() {
        var latch = CountDownLatch(1)
        var complete = false
        var error = false

        RustObject.safeAsync { }.then({
            complete = true
            latch.countDown()
            FxaResult<Void>()
        }, {
            error = true
            latch.countDown()
            FxaResult()
        })

        latch.await()
        assertFalse(error)
        assertTrue(complete)

        latch = CountDownLatch(1)
        complete = false

        RustObject.safeAsync { it.code = FxaClient.ErrorCode.AuthenticationError.ordinal }.then({
            complete = true
            latch.countDown()
            FxaResult<Void>()
        }, {
            assertTrue(it is FxaException.Unauthorized)
            error = true
            latch.countDown()
            FxaResult()
        })

        latch.await()
        assertTrue(error)
        assertFalse(complete)
    }

    @Test
    fun testSafeSync() {
        assertTrue(RustObject.safeSync { true })

        try {
            RustObject.safeSync { it.code = FxaClient.ErrorCode.AuthenticationError.ordinal }
            fail("Should have thrown FxaException.Unauthorized")
        } catch (e: FxaException.Unauthorized) { }

        try {
            RustObject.safeSync { it.code = FxaClient.ErrorCode.InternalPanic.ordinal }
            fail("Should have thrown FxaException.Panic")
        } catch (e: FxaException.Panic) { }

        try {
            RustObject.safeSync { it.code = FxaClient.ErrorCode.Other.ordinal }
            fail("Should have thrown FxaException.Unspecified")
        } catch (e: FxaException.Unspecified) { }
    }
}