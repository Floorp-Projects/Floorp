package mozilla.components.support.test.robolectric

import androidx.test.core.app.ApplicationProvider
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ExtensionsTest {

    @Test
    fun getProvidedAppContext() {
        Assert.assertEquals(
            ApplicationProvider.getApplicationContext(),
            testContext)
    }
}
