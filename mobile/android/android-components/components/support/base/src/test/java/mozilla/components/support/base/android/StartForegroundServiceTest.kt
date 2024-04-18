/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import android.os.Build
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class StartForegroundServiceTest {

    @Test
    fun `WHEN build version below S THEN start foreground service should return true regardless of foreground importance`() {
        val tested = StartForegroundService(
            FakeProcessInfoProvider(false),
            FakeBuildVersionProvider(Build.VERSION_CODES.P),
            FakePowerManagerInfoProvider(false),
        )

        var isInvoked = false
        val actual = tested.invoke {
            isInvoked = true
        }
        val expected = true

        assertEquals(expected, actual)
        assertTrue(isInvoked)
    }

    @Test
    fun `WHEN build version is S and above and foreground importance is false THEN start foreground service should return false`() {
        val tested = StartForegroundService(
            FakeProcessInfoProvider(false),
            FakeBuildVersionProvider(Build.VERSION_CODES.S),
            FakePowerManagerInfoProvider(false),
        )

        var isInvoked = false
        val actual = tested.invoke {
            isInvoked = true
        }

        assertFalse(actual)
        assertFalse(isInvoked)
    }

    @Test
    fun `WHEN build version is S and above and foreground importance is true THEN start foreground service should return true`() {
        val tested = StartForegroundService(
            FakeProcessInfoProvider(true),
            FakeBuildVersionProvider(Build.VERSION_CODES.S),
            FakePowerManagerInfoProvider(false),
        )

        var isInvoked = false
        val actual = tested.invoke {
            isInvoked = true
        }

        assertTrue(actual)
        assertTrue(isInvoked)
    }

    @Test
    fun `WHEN build version is S, foreground importance is false and battery optimisations are disabled THEN start foreground service should return true`() {
        val tested = StartForegroundService(
            FakeProcessInfoProvider(false),
            FakeBuildVersionProvider(Build.VERSION_CODES.S),
            FakePowerManagerInfoProvider(true),
        )

        var isInvoked = true
        val actual = tested.invoke {
            isInvoked = true
        }

        assertTrue(actual)
        assertTrue(isInvoked)
    }

    class FakeProcessInfoProvider(private val isForegroundImportance: Boolean) :
        ProcessInfoProvider {
        override fun isForegroundImportance(): Boolean = isForegroundImportance
    }

    class FakeBuildVersionProvider(private val sdkInt: Int) : BuildVersionProvider {
        override fun sdkInt(): Int = sdkInt
    }

    class FakePowerManagerInfoProvider(
        private val isIgnoringBatteryOptimizations: Boolean,
    ) : PowerManagerInfoProvider {
        override fun isIgnoringBatteryOptimizations(): Boolean = isIgnoringBatteryOptimizations
    }
}
