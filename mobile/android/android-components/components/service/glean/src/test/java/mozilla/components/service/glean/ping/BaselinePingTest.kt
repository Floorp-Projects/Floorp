/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.ping

import android.accessibilityservice.AccessibilityServiceInfo
import android.content.Context
import android.view.accessibility.AccessibilityManager
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows

@RunWith(RobolectricTestRunner::class)
class BaselinePingTest {

    @Test
    fun `Make sure a11y services are collected`() {
        val applicationContext = ApplicationProvider.getApplicationContext<Context>()
        val accessibilityManager = applicationContext.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
        val baselinePing = BaselinePing(applicationContext)

        val shadowAccessibilityManager = Shadows.shadowOf(accessibilityManager)
        shadowAccessibilityManager.setEnabled(true)

        val serviceSpy1 = Mockito.spy<AccessibilityServiceInfo>(AccessibilityServiceInfo::class.java)
        Mockito.doReturn("service1").`when`(serviceSpy1).id

        val serviceSpy2 = Mockito.spy<AccessibilityServiceInfo>(AccessibilityServiceInfo::class.java)
        Mockito.doReturn("service2").`when`(serviceSpy2).id

        val nullServiceId = Mockito.spy<AccessibilityServiceInfo>(AccessibilityServiceInfo::class.java)
        Mockito.doReturn(null).`when`(nullServiceId).id

        shadowAccessibilityManager.setEnabledAccessibilityServiceList(
            listOf(serviceSpy1, serviceSpy2, nullServiceId)
        )

        assertEquals(
            listOf("service1", "service2"),
            baselinePing.getEnabledAccessibilityServices(
                accessibilityManager
            )
        )
    }
}
