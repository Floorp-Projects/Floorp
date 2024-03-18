/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertTrue
import junit.framework.TestCase.fail
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import mozilla.components.test.ReflectionUtils
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntime
import java.security.InvalidParameterException
import org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice as GeckoRecordingDevice

@RunWith(AndroidJUnit4::class)
class GeckoMediaDelegateTest {
    private lateinit var runtime: GeckoRuntime

    @Before
    fun setup() {
        runtime = mock()
        whenever(runtime.settings).thenReturn(mock())
    }

    @Test
    fun `WHEN onRecordingStatusChanged is called THEN notify onRecordingStateChanged`() {
        val mockSession = GeckoEngineSession(runtime)
        var onRecordingWasCalled = false
        val geckoRecordingDevice = createGeckoRecordingDevice(
            status = GeckoRecordingDevice.Status.RECORDING,
            type = GeckoRecordingDevice.Type.CAMERA,
        )
        val gecko = GeckoMediaDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onRecordingStateChanged(devices: List<RecordingDevice>) {
                    onRecordingWasCalled = true
                }
            },
        )

        gecko.onRecordingStatusChanged(mock(), arrayOf(geckoRecordingDevice))

        assertTrue(onRecordingWasCalled)
    }

    @Test
    fun `GIVEN a GeckoRecordingDevice status WHEN calling toStatus THEN covert to the RecordingDevice status`() {
        val geckoRecordingDevice = createGeckoRecordingDevice(
            status = GeckoRecordingDevice.Status.RECORDING,
        )
        val geckoInactiveDevice = createGeckoRecordingDevice(
            status = GeckoRecordingDevice.Status.INACTIVE,
        )

        assertEquals(RecordingDevice.Status.RECORDING, geckoRecordingDevice.toStatus())
        assertEquals(RecordingDevice.Status.INACTIVE, geckoInactiveDevice.toStatus())
    }

    @Test
    fun `GIVEN an invalid GeckoRecordingDevice status WHEN calling toStatus THEN throw an exception`() {
        val geckoInvalidDevice = createGeckoRecordingDevice(
            status = 12,
        )
        try {
            geckoInvalidDevice.toStatus()
            fail()
        } catch (_: InvalidParameterException) {
        }
    }

    @Test
    fun `GIVEN a GeckoRecordingDevice type WHEN calling toType THEN covert to the RecordingDevice type`() {
        val geckoCameraDevice = createGeckoRecordingDevice(
            type = GeckoRecordingDevice.Type.CAMERA,
        )
        val geckoMicDevice = createGeckoRecordingDevice(
            type = GeckoRecordingDevice.Type.MICROPHONE,
        )

        assertEquals(RecordingDevice.Type.CAMERA, geckoCameraDevice.toType())
        assertEquals(RecordingDevice.Type.MICROPHONE, geckoMicDevice.toType())
    }

    @Test
    fun `GIVEN an invalid GeckoRecordingDevice type WHEN calling toType THEN throw an exception`() {
        val geckoInvalidDevice = createGeckoRecordingDevice(
            type = 12,
        )
        try {
            geckoInvalidDevice.toType()
            fail()
        } catch (_: InvalidParameterException) {
        }
    }

    private fun createGeckoRecordingDevice(
        status: Long = GeckoRecordingDevice.Status.RECORDING,
        type: Long = GeckoRecordingDevice.Type.CAMERA,
    ): GeckoRecordingDevice {
        val device: GeckoRecordingDevice = mock()
        ReflectionUtils.setField(device, "status", status)
        ReflectionUtils.setField(device, "type", type)
        return device
    }
}
