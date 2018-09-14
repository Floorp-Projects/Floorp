package mozilla.components.support.ktx.android.content.res

import android.util.DisplayMetrics
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class ResourcesTest {

    @Test
    fun `dp returns same value as manual conversion`() {

        val resources = RuntimeEnvironment.application.resources
        val metrics = resources.displayMetrics

        for (i in 1..500) {
            val px = (i * (metrics.densityDpi.toFloat() / DisplayMetrics.DENSITY_DEFAULT)).toInt()
            Assert.assertEquals(px, resources.pxToDp(i))
            Assert.assertNotEquals(0, resources.pxToDp(i))
        }
    }
}