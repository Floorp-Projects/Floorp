/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MediaElement
import org.mozilla.geckoview.MockRecordingDevice

@RunWith(AndroidJUnit4::class)
class GeckoMediaDelegateTest {

    @Test
    fun `Added MediaElement is wrapped in GeckoMedia and forwarded to observer`() {
        val engineSession = GeckoEngineSession(mock())

        var observedMedia: Media? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onMediaAdded(media: Media) {
                observedMedia = media
            }
        })

        val mediaElement: MediaElement = mock()
        engineSession.geckoSession.mediaDelegate!!.onMediaAdd(mock(), mediaElement)

        assertNotNull(observedMedia!!)

        observedMedia!!.controller.play()
        verify(mediaElement).play()
    }

    @Test
    fun `WHEN MediaElement is removed THEN previously added GeckoMedia is used to notify observer`() {
        val engineSession = GeckoEngineSession(mock())

        var addedMedia: Media? = null
        var removedMedia: Media? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onMediaAdded(media: Media) {
                addedMedia = media
            }

            override fun onMediaRemoved(media: Media) {
                removedMedia = media
            }
        })

        val mediaElement: MediaElement = mock()
        engineSession.geckoSession.mediaDelegate!!.onMediaAdd(mock(), mediaElement)
        engineSession.geckoSession.mediaDelegate!!.onMediaRemove(mock(), mediaElement)

        assertNotNull(addedMedia!!)
        assertNotNull(removedMedia!!)
        assertEquals(addedMedia, removedMedia)
    }

    @Test
    fun `WHEN unknown media is removed THEN observer is not notified`() {
        val engineSession = GeckoEngineSession(mock())

        var onMediaRemovedExecuted = false

        engineSession.register(object : EngineSession.Observer {
            override fun onMediaRemoved(media: Media) {
                onMediaRemovedExecuted = true
            }
        })

        val mediaElement: MediaElement = mock()
        engineSession.geckoSession.mediaDelegate!!.onMediaRemove(mock(), mediaElement)

        assertFalse(onMediaRemovedExecuted)
    }

    @Test
    fun `WHEN recording state changes THEN observers are notified`() {
        val engineSession = GeckoEngineSession(mock())

        var observedDevices: List<RecordingDevice> = listOf()

        engineSession.register(object : EngineSession.Observer {
            override fun onRecordingStateChanged(devices: List<RecordingDevice>) {
                observedDevices = devices
            }
        })

        assertTrue(observedDevices.isEmpty())

        engineSession.geckoSession.mediaDelegate!!.onRecordingStatusChanged(mock(), arrayOf(
            MockRecordingDevice(
                GeckoSession.MediaDelegate.RecordingDevice.Type.CAMERA,
                GeckoSession.MediaDelegate.RecordingDevice.Status.RECORDING)
        ))

        assertEquals(1, observedDevices.size)
        assertEquals(RecordingDevice(RecordingDevice.Type.CAMERA, RecordingDevice.Status.RECORDING), observedDevices[0])

        engineSession.geckoSession.mediaDelegate!!.onRecordingStatusChanged(mock(), arrayOf(
            MockRecordingDevice(
                GeckoSession.MediaDelegate.RecordingDevice.Type.CAMERA,
                GeckoSession.MediaDelegate.RecordingDevice.Status.RECORDING),
            MockRecordingDevice(
                GeckoSession.MediaDelegate.RecordingDevice.Type.MICROPHONE,
                GeckoSession.MediaDelegate.RecordingDevice.Status.INACTIVE)
        ))

        assertEquals(2, observedDevices.size)
        assertEquals(RecordingDevice(RecordingDevice.Type.CAMERA, RecordingDevice.Status.RECORDING), observedDevices[0])
        assertEquals(RecordingDevice(RecordingDevice.Type.MICROPHONE, RecordingDevice.Status.INACTIVE), observedDevices[1])

        engineSession.geckoSession.mediaDelegate!!.onRecordingStatusChanged(mock(), emptyArray())

        assertTrue(observedDevices.isEmpty())
    }
}
