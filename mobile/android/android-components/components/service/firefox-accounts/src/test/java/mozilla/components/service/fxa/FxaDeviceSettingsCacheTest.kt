/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.appservices.syncmanager.DeviceSettings
import mozilla.appservices.syncmanager.DeviceType
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import java.lang.IllegalStateException

@RunWith(AndroidJUnit4::class)
class FxaDeviceSettingsCacheTest {
    @Test
    fun `fxa device settings cache basics`() {
        val cache = FxaDeviceSettingsCache(testContext)
        assertNull(cache.getCached())

        try {
            cache.updateCachedName("new name")
            fail()
        } catch (e: IllegalStateException) {}

        cache.setToCache(DeviceSettings("some id", "some name", DeviceType.VR))
        assertEquals(
            DeviceSettings("some id", "some name", DeviceType.VR),
            cache.getCached(),
        )

        cache.updateCachedName("new name")
        assertEquals(
            DeviceSettings("some id", "new name", DeviceType.VR),
            cache.getCached(),
        )

        cache.clear()
        assertNull(cache.getCached())

        cache.setToCache(DeviceSettings("some id", "mobile", DeviceType.MOBILE))
        assertEquals(
            DeviceSettings("some id", "mobile", DeviceType.MOBILE),
            cache.getCached(),
        )

        cache.setToCache(DeviceSettings("some id", "some tv", DeviceType.TV))
        assertEquals(
            DeviceSettings("some id", "some tv", DeviceType.TV),
            cache.getCached(),
        )

        cache.setToCache(DeviceSettings("some id", "some tablet", DeviceType.TABLET))
        assertEquals(
            DeviceSettings("some id", "some tablet", DeviceType.TABLET),
            cache.getCached(),
        )

        cache.setToCache(DeviceSettings("some id", "some desktop", DeviceType.DESKTOP))
        assertEquals(
            DeviceSettings("some id", "some desktop", DeviceType.DESKTOP),
            cache.getCached(),
        )
    }
}
