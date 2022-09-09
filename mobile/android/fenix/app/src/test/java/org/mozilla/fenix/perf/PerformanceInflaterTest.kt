/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import java.io.File

@RunWith(FenixRobolectricTestRunner::class)
class PerformanceInflaterTest {

    private lateinit var perfInflater: MockInflater

    private val layoutsNotToTest = setOf(
        "fragment_browser",
        "fragment_add_on_internal_settings",
        "activity_privacy_content_display",
        /**
         *  activity_home.xml contains FragmentContainerView which needs to be
         *  put inside FragmentActivity in order to get inflated
         */
        "activity_home",
    )

    @Before
    fun setup() {
        InflationCounter.inflationCount.set(0)

        every { testContext.components.core.engine.profiler } returns mockk(relaxed = true)
        perfInflater = MockInflater(LayoutInflater.from(testContext), testContext)
    }

    @Test
    fun `WHEN we inflate a view,THEN the inflation counter should increase`() {
        assertEquals(0, InflationCounter.inflationCount.get())
        perfInflater.inflate(R.layout.fragment_home, null, false)
        assertEquals(1, InflationCounter.inflationCount.get())
    }

    @Test
    fun `WHEN inflating one of our resource file, the inflater should not crash`() {
        val fileList = File("./src/main/res/layout").listFiles()

        // There might be custom views who try to access `Settings` through the extension function.
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk(relaxed = true)

            for (file in fileList!!) {
                val layoutName = file.name.split(".")[0]
                val layoutId = testContext.resources.getIdentifier(
                    layoutName,
                    "layout",
                    testContext.packageName,
                )

                assertNotEquals(-1, layoutId)
                if (!layoutsNotToTest.contains(layoutName)) {
                    perfInflater.inflate(layoutId, FrameLayout(testContext), true)
                }
            }
        }
    }
}

private class MockInflater(
    inflater: LayoutInflater,
    context: Context,
) : PerformanceInflater(
    inflater,
    context,
) {

    override fun onCreateView(name: String?, attrs: AttributeSet?): View? {
        // We skip the fragment layout for the simple reason that it implements
        // a whole different inflate which is implemented in the activity.LayoutFactory
        // methods. To be able to properly test it here, we would have to copy the whole
        // inflater file (or create an activity) and pass our layout through the onCreateView
        // method of that activity.
        if (name!!.contains("fragment")) {
            return FrameLayout(testContext)
        }
        return super.onCreateView(name, attrs)
    }
}
