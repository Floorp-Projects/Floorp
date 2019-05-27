/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.media.RecordingDevice
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MediaElement
import org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice as GeckoRecordingDevice

/**
 * [GeckoSession.MediaDelegate] implementation for wrapping [MediaElement] instances in [GeckoMedia] ([Media]) and
 * forwarding them to the [EngineSession.Observer].
 */
internal class GeckoMediaDelegate(
    private val engineSession: GeckoEngineSession
) : GeckoSession.MediaDelegate {
    private val mediaMap: MutableMap<MediaElement, Media> = mutableMapOf()

    override fun onMediaAdd(session: GeckoSession, element: MediaElement) {
        val media = GeckoMedia(element).also {
            mediaMap[element] = it
        }

        engineSession.notifyObservers { onMediaAdded(media) }
    }

    override fun onMediaRemove(session: GeckoSession, element: MediaElement) {
        mediaMap[element]?.let { media ->
            engineSession.notifyObservers { onMediaRemoved(media) }
        }
    }

    override fun onRecordingStatusChanged(
        session: GeckoSession,
        devices: Array<out GeckoRecordingDevice>
    ) {
        val genericDevices = devices.map { it.toRecordingDevice() }
        engineSession.notifyObservers {
            onRecordingStateChanged(genericDevices)
        }
    }
}

private fun GeckoRecordingDevice.toRecordingDevice(): RecordingDevice {
    val type = when (type) {
        GeckoRecordingDevice.Type.CAMERA -> RecordingDevice.Type.CAMERA
        GeckoRecordingDevice.Type.MICROPHONE -> RecordingDevice.Type.MICROPHONE
        else -> throw IllegalStateException("Unknown device type: $type")
    }

    val status = when (status) {
        GeckoRecordingDevice.Status.INACTIVE -> RecordingDevice.Status.INACTIVE
        GeckoRecordingDevice.Status.RECORDING -> RecordingDevice.Status.RECORDING
        else -> throw IllegalStateException("Unknown device status: $status")
    }

    return RecordingDevice(type, status)
}
