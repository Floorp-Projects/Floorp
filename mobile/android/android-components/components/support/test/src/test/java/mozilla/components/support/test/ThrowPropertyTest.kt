package mozilla.components.support.test.robolectric.mozilla.components.support.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.ThrowProperty
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ThrowPropertyTest {
    private val testProperty = "test"

    @Test(expected = UnsupportedOperationException::class)
    fun `exception thrown when get value is called`() {
        val throwProperty = ThrowProperty<String>()
        throwProperty.getValue(testProperty, ::testProperty)
    }

    @Test(expected = UnsupportedOperationException::class)
    fun `exception thrown when set value is called`() {
        val throwProperty = ThrowProperty<String>()
        throwProperty.setValue(testProperty, ::testProperty, "test1")
    }
}
