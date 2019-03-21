package mozilla.components.support.ktx.android.content.res

import android.content.Context
import android.util.DisplayMetrics
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ResourcesTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `dp returns same value as manual conversion`() {
        val resources = context.resources
        val metrics = resources.displayMetrics

        for (i in 1..500) {
            val px = (i * (metrics.densityDpi.toFloat() / DisplayMetrics.DENSITY_DEFAULT)).toInt()
            Assert.assertEquals(px, resources.pxToDp(i))
            Assert.assertNotEquals(0, resources.pxToDp(i))
        }
    }
}