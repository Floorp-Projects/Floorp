package mozilla.components.support.ktx.android.util

import android.util.DisplayMetrics
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DisplayMetricsTest {

    @Test
    fun `dp returns same value as manual conversion`() {
        val metrics = testContext.resources.displayMetrics

        for (i in 1..500) {
            val px = (i * (metrics.densityDpi.toFloat() / DisplayMetrics.DENSITY_DEFAULT)).toInt()
            Assert.assertEquals(px, i.dpToPx(metrics))
            Assert.assertNotEquals(0, i.dpToPx(metrics))
        }
    }
}
