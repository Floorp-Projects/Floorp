/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.util

import android.content.res.Resources
import android.util.DisplayMetrics
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.android.util.spToPx
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class DisplayMetricsTest {
    private lateinit var metrics: DisplayMetrics

    @Before
    fun setUp() {
        metrics = mock(DisplayMetrics::class.java)
        metrics.density = 3f
        metrics.scaledDensity = 2f

        val resources: Resources = mock(Resources::class.java)
        `when`(resources.displayMetrics).thenReturn(metrics)
    }

    @Test
    fun `Float dpToPx returns correct value`() {
        val floatValue = 10f

        val result = floatValue.dpToPx(metrics)

        assertEquals(metrics.density * floatValue, result)
    }

    @Test
    fun `Float spToPx returns correct value`() {
        val floatValue = 10f

        val result = floatValue.spToPx(metrics)

        assertEquals(metrics.scaledDensity * floatValue, result)
    }
}
