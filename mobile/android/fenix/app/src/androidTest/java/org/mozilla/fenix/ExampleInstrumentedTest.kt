package org.mozilla.fenix

import android.support.test.InstrumentationRegistry
import android.support.test.runner.AndroidJUnit4
import androidx.test.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4

import org.junit.Test
import org.junit.runner.RunWith
/* ktlint-disable no-wildcard-imports */
import org.junit.Assert.*

/**
 * Instrumented test, which will execute on an Android device.
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
@RunWith(AndroidJUnit4::class)
class ExampleInstrumentedTest {
    @Test
    fun useAppContext() {
        // Context of the app under test.
        val appContext = InstrumentationRegistry.getTargetContext()
        assertEquals("org.mozilla.fenix", appContext.packageName)
    }
}
