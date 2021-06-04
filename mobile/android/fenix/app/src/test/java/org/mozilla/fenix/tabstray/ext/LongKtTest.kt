package org.mozilla.fenix.tabstray.ext

import org.junit.Assert.assertEquals
import org.junit.Test

class LongKtTest {

    @Test
    fun `WHEN value is null THEN default is returned`() {
        val value: Long? = null

        assertEquals(value.orDefault(), -1L)
    }

    @Test
    fun `WHEN value is not null THEN value is returned`() {
        val value: Long? = 100L

        assertEquals(value.orDefault(), 100L)
    }
}
