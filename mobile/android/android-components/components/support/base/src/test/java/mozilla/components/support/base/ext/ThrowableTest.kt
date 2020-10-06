/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ext

import org.junit.Assert.assertEquals
import org.junit.Test
import java.lang.RuntimeException

class ThrowableTest {
    @Test
    fun `throwable stack trace to string is limited to max length`() {
        val throwable = RuntimeException("TEST ONLY")

        assertEquals(throwable.getStacktraceAsString(1).length, 1)
        assertEquals(throwable.getStacktraceAsString(10).length, 10)
    }

    @Test
    fun `throwable stack trace to string works correctly`() {
        val throwable = RuntimeException("TEST ONLY")

        assert(throwable.getStacktraceAsString().contains("mozilla.components.support.base.ext.ThrowableTest.throwable stack trace to string works correctly(ThrowableTest.kt:"))
    }
}
