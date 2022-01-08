/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import android.view.ViewStub
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.junit.Test
import java.lang.ref.WeakReference

class InflationAwareFeatureTest {
    @Test
    fun `stub inflates if no feature or view exists`() {
        val stub: ViewStub = mockk(relaxed = true)
        val feature: InflationAwareFeature = spyk(TestableInflationAwareFeature(stub))

        feature.launch()

        verify { stub.setOnInflateListener(any()) }
        verify { stub.inflate() }
    }

    @Test
    fun `stub immediately launches if the feature is available`() {
        val stub: ViewStub = mockk()
        val feature: InflationAwareFeature = spyk(TestableInflationAwareFeature(stub))

        feature.feature = mockk(relaxed = true)
        feature.view = WeakReference(mockk())

        feature.launch()

        verify(exactly = 0) { stub.setOnInflateListener(any()) }
        verify(exactly = 0) { stub.inflate() }
        verify { feature.onLaunch(any(), any()) }
    }

    @Test
    fun `feature calls stop if created`() {
        val stub: ViewStub = mockk()
        val inflationFeature: InflationAwareFeature = spyk(TestableInflationAwareFeature(stub))
        val innerFeature: LifecycleAwareFeature = mockk(relaxed = true)

        inflationFeature.stop()

        verify(exactly = 0) { innerFeature.stop() }

        inflationFeature.feature = innerFeature

        inflationFeature.stop()

        verify { innerFeature.stop() }
    }

    @Test
    fun `start should be delegated to the inner feature`() {
        val inflationFeature: InflationAwareFeature = spyk(TestableInflationAwareFeature(mockk()))
        val innerFeature: LifecycleAwareFeature = mockk(relaxed = true)
        inflationFeature.feature = innerFeature

        inflationFeature.start()

        verify { innerFeature.start() }
    }

    @Test
    fun `if feature has implemented UserInteractionHandler invoke it`() {
        val stub: ViewStub = mockk()
        val inflationFeature: InflationAwareFeature = spyk(TestableInflationAwareFeature(stub))
        val innerFeature: LifecycleAwareFeature = mockk()
        val userInteractionHandlerFeature = object : LifecycleAwareFeature, UserInteractionHandler {
            override fun onBackPressed() = true

            override fun start() {}

            override fun stop() {}
        }

        assert(!inflationFeature.onBackPressed())

        inflationFeature.feature = innerFeature

        assert(!inflationFeature.onBackPressed())

        inflationFeature.feature = userInteractionHandlerFeature

        assert(inflationFeature.onBackPressed())
    }
}

class TestableInflationAwareFeature(stub: ViewStub) : InflationAwareFeature(stub) {
    override fun onViewInflated(view: View): LifecycleAwareFeature = mockk()

    override fun onLaunch(view: View, feature: LifecycleAwareFeature) = Unit
}
