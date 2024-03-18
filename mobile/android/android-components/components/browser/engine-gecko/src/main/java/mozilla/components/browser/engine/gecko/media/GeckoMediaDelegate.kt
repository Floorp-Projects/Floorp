/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.media.RecordingDevice
import org.mozilla.geckoview.GeckoSession
import java.security.InvalidParameterException
import org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice as GeckoRecordingDevice

/**
 * Gecko-based GeckoMediaDelegate implementation.
 */
internal class GeckoMediaDelegate(private val geckoEngineSession: GeckoEngineSession) :
    GeckoSession.MediaDelegate {

    override fun onRecordingStatusChanged(
        session: GeckoSession,
        geckoDevices: Array<out GeckoRecordingDevice>,
    ) {
        val devices = geckoDevices.map { geckoRecording ->
            val type = geckoRecording.toType()
            val status = geckoRecording.toStatus()
            RecordingDevice(type, status)
        }
        geckoEngineSession.notifyObservers { onRecordingStateChanged(devices) }
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun GeckoRecordingDevice.toType(): RecordingDevice.Type {
    return when (type) {
        GeckoRecordingDevice.Type.CAMERA -> RecordingDevice.Type.CAMERA
        GeckoRecordingDevice.Type.MICROPHONE -> RecordingDevice.Type.MICROPHONE
        else -> {
            throw InvalidParameterException("Unexpected Gecko Media type $type status $status")
        }
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun GeckoRecordingDevice.toStatus(): RecordingDevice.Status {
    return when (status) {
        GeckoRecordingDevice.Status.RECORDING -> RecordingDevice.Status.RECORDING
        GeckoRecordingDevice.Status.INACTIVE -> RecordingDevice.Status.INACTIVE
        else -> {
            throw InvalidParameterException("Unexpected Gecko Media type $type status $status")
        }
    }
}
