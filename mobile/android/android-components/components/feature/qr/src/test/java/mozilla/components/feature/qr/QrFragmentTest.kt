/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr

import android.hardware.camera2.CameraDevice
import android.util.Size
import com.google.zxing.BarcodeFormat
import com.google.zxing.BinaryBitmap
import com.google.zxing.MultiFormatReader
import com.google.zxing.NotFoundException
import mozilla.components.support.base.android.view.AutoFitTextureView
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class QrFragmentTest {

    @Test
    fun `initialize QR fragment`() {
        val scanCompleteListener = mock(QrFragment.OnScanCompleteListener::class.java)
        val qrFragment = spy(QrFragment.newInstance(scanCompleteListener))

        qrFragment.scanCompleteListener?.onScanComplete("result")
        verify(scanCompleteListener).onScanComplete("result")
    }

    @Test
    fun `onPause closes camera and stops background thread`() {
        val qrFragment = spy(QrFragment.newInstance(mock(QrFragment.OnScanCompleteListener::class.java)))
        qrFragment.onPause()

        verify(qrFragment).stopBackgroundThread()
        verify(qrFragment).closeCamera()
    }

    @Test
    fun `onResume opens camera and starts background thread`() {
        val qrFragment = spy(QrFragment.newInstance(mock(QrFragment.OnScanCompleteListener::class.java)))
        `when`(qrFragment.setUpCameraOutputs(anyInt(), anyInt())).then { }

        qrFragment.textureView = mock()
        qrFragment.onResume()
        verify(qrFragment, never()).openCamera(anyInt(), anyInt())

        `when`(qrFragment.textureView.isAvailable).thenReturn(true)
        qrFragment.onResume()
        verify(qrFragment, times(2)).startBackgroundThread()
        verify(qrFragment).openCamera(anyInt(), anyInt())
    }

    @Test
    fun `onViewCreated sets initial state`() {
        val qrFragment = QrFragment.newInstance(mock(QrFragment.OnScanCompleteListener::class.java))
        val view: AutoFitTextureView = mock()
        `when`(view.findViewById<AutoFitTextureView>(anyInt())).thenReturn(mock())

        qrFragment.onViewCreated(view, mock())
        assertEquals(QrFragment.STATE_FIND_QRCODE, QrFragment.qrState)
    }

    @Test
    fun `async scanning task invokes listener on successful qr scan`() {
        val listener = mock(QrFragment.OnScanCompleteListener::class.java)
        val reader = mock(MultiFormatReader::class.java)
        val task = QrFragment.AsyncScanningTask(listener, reader)

        val bitmap = mock(BinaryBitmap::class.java)
        val result = com.google.zxing.Result("qrcode-result", ByteArray(0), emptyArray(), BarcodeFormat.ITF)
        `when`(reader.decodeWithState(any())).thenReturn(result)

        QrFragment.qrState = QrFragment.STATE_DECODE_PROGRESS
        task.processImage(bitmap)

        verify(listener).onScanComplete(eq("qrcode-result"))
        assertEquals(QrFragment.STATE_QRCODE_EXIST, QrFragment.qrState)
    }

    @Test
    fun `async scanning task resets state on error`() {
        val listener = mock(QrFragment.OnScanCompleteListener::class.java)
        val reader = mock(MultiFormatReader::class.java)
        val task = QrFragment.AsyncScanningTask(listener, reader)

        val bitmap = mock(BinaryBitmap::class.java)
        `when`(reader.decodeWithState(any())).thenThrow(NotFoundException::class.java)

        QrFragment.qrState = QrFragment.STATE_DECODE_PROGRESS
        task.processImage(bitmap)

        assertEquals(QrFragment.STATE_FIND_QRCODE, QrFragment.qrState)
    }

    @Test
    fun `async scanning task does nothing if decoding not in progress`() {
        val listener = mock(QrFragment.OnScanCompleteListener::class.java)
        val reader = mock(MultiFormatReader::class.java)
        val task = QrFragment.AsyncScanningTask(listener, reader)

        val bitmap = mock(BinaryBitmap::class.java)
        `when`(reader.decodeWithState(any())).thenThrow(NotFoundException::class.java)

        QrFragment.qrState = QrFragment.STATE_FIND_QRCODE
        assertNull(task.processImage(bitmap))
        verify(reader, never()).decodeWithState(any())
    }

    @Test
    fun `camera is closed on disconnect and error`() {
        val qrFragment = spy(QrFragment.newInstance(mock(QrFragment.OnScanCompleteListener::class.java)))

        var camera: CameraDevice = mock()
        qrFragment.stateCallback.onDisconnected(camera)
        verify(camera).close()

        camera = mock()
        qrFragment.stateCallback.onError(camera, 0)
        verify(camera).close()
    }

    @Test
    fun `choose optimal size`() {
        var size = QrFragment.chooseOptimalSize(
            arrayOf(Size(640, 480), Size(1024, 768)),
            640,
            480,
            QrFragment.MAX_PREVIEW_WIDTH,
            QrFragment.MAX_PREVIEW_HEIGHT,
            Size(16, 9)
        )

        assertEquals(640, size.width)
        assertEquals(480, size.height)

        size = QrFragment.chooseOptimalSize(
            arrayOf(Size(1024, 768), Size(640, 480)),
            1024,
            768,
            QrFragment.MAX_PREVIEW_WIDTH,
            QrFragment.MAX_PREVIEW_HEIGHT,
            Size(4, 3)
        )

        assertEquals(1024, size.width)
        assertEquals(768, size.height)

        size = QrFragment.chooseOptimalSize(
            arrayOf(Size(1024, 768), Size(640, 480), Size(320, 240)),
            2048,
            768,
            QrFragment.MAX_PREVIEW_WIDTH,
            QrFragment.MAX_PREVIEW_HEIGHT,
            Size(4, 3)
        )

        assertEquals(1024, size.width)
        assertEquals(768, size.height)

        size = QrFragment.chooseOptimalSize(
            arrayOf(Size(1024, 768), Size(640, 480), Size(320, 240)),
            1024,
            1024,
            QrFragment.MAX_PREVIEW_WIDTH,
            QrFragment.MAX_PREVIEW_HEIGHT,
            Size(4, 3)
        )

        assertEquals(1024, size.width)
        assertEquals(768, size.height)

        size = QrFragment.chooseOptimalSize(
            arrayOf(Size(1024, 768), Size(640, 480), Size(320, 240)),
            2048,
            1024,
            QrFragment.MAX_PREVIEW_WIDTH,
            QrFragment.MAX_PREVIEW_HEIGHT,
            Size(16, 9)
        )

        assertEquals(1024, size.width)
        assertEquals(768, size.height)
    }
}