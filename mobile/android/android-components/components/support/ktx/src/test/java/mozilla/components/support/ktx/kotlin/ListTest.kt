package mozilla.components.support.ktx.kotlin

import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ListTest {
    @Test
    fun testToJsonArrayEmptyList() {
        val jsonArray = listOf<String>().toJsonArray()
        assertEquals(0, jsonArray.length())
    }

    @Test
    fun testToJsonArrayNotEmptyList() {
        val list = listOf(1, 2, 3)
        val jsonArray = list.toJsonArray()
        assertEquals(3, jsonArray.length())
        assertEquals(1, jsonArray[0])
        assertEquals(2, jsonArray[1])
        assertEquals(3, jsonArray[2])
    }
}
