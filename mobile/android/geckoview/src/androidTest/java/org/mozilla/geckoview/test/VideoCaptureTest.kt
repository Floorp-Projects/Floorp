package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.SmallTest
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.webrtc.CameraEnumerationAndroid.CaptureFormat
import org.webrtc.CameraEnumerator
import org.webrtc.CameraVideoCapturer
import org.webrtc.CameraVideoCapturer.CameraEventsHandler
import org.webrtc.videoengine.VideoCaptureAndroid

@RunWith(AndroidJUnit4::class)
@SmallTest
class VideoCaptureTest {
    // Always throw exception.
    class BadCameraEnumerator : CameraEnumerator {
        override fun getDeviceNames(): Array<String?>? {
            throw java.lang.RuntimeException("")
        }

        override fun isFrontFacing(deviceName: String?): Boolean {
            throw java.lang.RuntimeException("")
        }

        override fun isBackFacing(deviceName: String?): Boolean {
            throw java.lang.RuntimeException("")
        }

        override fun isInfrared(deviceName: String?): Boolean {
            throw java.lang.RuntimeException("")
        }

        override fun getSupportedFormats(deviceName: String?): List<CaptureFormat?>? {
            throw java.lang.RuntimeException("")
        }

        override fun createCapturer(
            deviceName: String?,
            eventsHandler: CameraEventsHandler?,
        ): CameraVideoCapturer? {
            throw java.lang.RuntimeException("")
        }
    }

    @Test
    fun constructWithBadEnumerator() {
        val ctr = VideoCaptureAndroid::class.java.getDeclaredConstructors()[0].apply { isAccessible = true }
        val capture = ctr.newInstance(
            InstrumentationRegistry.getInstrumentation().targetContext,
            "my camera",
            BadCameraEnumerator(),
        ) as VideoCaptureAndroid
        assertEquals(false, capture.canCapture())
    }
}
