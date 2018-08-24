package mozilla.components.concept.engine

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class HitResultTest {
    @Test
    fun testConstructor() {
        var result: HitResult = HitResult.UNKNOWN("file://foobar")
        assertTrue(result is HitResult.UNKNOWN)
        assertEquals(result.src, "file://foobar")

        result = HitResult.IMAGE("https://mozilla.org/i.png")
        assertEquals(result.src, "https://mozilla.org/i.png")

        result = HitResult.IMAGE_SRC("https://mozilla.org/i.png", "https://firefox.com")
        assertEquals(result.src, "https://mozilla.org/i.png")
        assertEquals(result.uri, "https://firefox.com")
    }
}