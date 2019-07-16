/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr

import android.content.Context
import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.media.Image
import android.util.Size
import androidx.fragment.app.FragmentActivity
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.zxing.BarcodeFormat
import com.google.zxing.BinaryBitmap
import com.google.zxing.MultiFormatReader
import com.google.zxing.NotFoundException
import mozilla.components.support.base.android.view.AutoFitTextureView
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.nio.ByteBuffer

@RunWith(AndroidJUnit4::class)
class QrFragmentTest {

    @Test
    fun `initialize QR fragment`() {
        val scanCompleteListener = mock<QrFragment.OnScanCompleteListener>()
        val qrFragment = spy(QrFragment.newInstance(scanCompleteListener))

        qrFragment.scanCompleteListener?.onScanComplete("result")
        verify(scanCompleteListener).onScanComplete("result")
    }

    @Test
    fun `onPause closes camera and stops background thread`() {
        val qrFragment = spy(QrFragment.newInstance(mock()))
        qrFragment.onPause()

        verify(qrFragment).stopBackgroundThread()
        verify(qrFragment).closeCamera()
    }

    @Test
    fun `onResume opens camera and starts background thread`() {
        val qrFragment = spy(QrFragment.newInstance(mock()))
        whenever(qrFragment.setUpCameraOutputs(anyInt(), anyInt())).then { }

        qrFragment.textureView = mock()
        qrFragment.onResume()
        verify(qrFragment, never()).openCamera(anyInt(), anyInt())

        whenever(qrFragment.textureView.isAvailable).thenReturn(true)
        qrFragment.cameraId = "mockCamera"
        qrFragment.onResume()
        verify(qrFragment, times(2)).startBackgroundThread()
        verify(qrFragment).openCamera(anyInt(), anyInt())
    }

    @Test
    fun `onViewCreated sets initial state`() {
        val qrFragment = QrFragment.newInstance(mock())
        val view: AutoFitTextureView = mock()
        whenever(view.findViewById<AutoFitTextureView>(anyInt())).thenReturn(mock())

        qrFragment.onViewCreated(view, mock())
        assertEquals(QrFragment.STATE_FIND_QRCODE, QrFragment.qrState)
    }

    @Test
    fun `async scanning task invokes listener on successful qr scan`() {
        val listener = mock<QrFragment.OnScanCompleteListener>()
        val reader = mock<MultiFormatReader>()
        val task = QrFragment.AsyncScanningTask(listener, reader)

        val bitmap = mock<BinaryBitmap>()
        val result = com.google.zxing.Result("qrcode-result", ByteArray(0), emptyArray(), BarcodeFormat.ITF)
        whenever(reader.decodeWithState(any())).thenReturn(result)

        QrFragment.qrState = QrFragment.STATE_DECODE_PROGRESS
        task.processImage(bitmap)

        verify(listener).onScanComplete(eq("qrcode-result"))
        assertEquals(QrFragment.STATE_QRCODE_EXIST, QrFragment.qrState)
    }

    @Test
    fun `async scanning task resets state on error`() {
        val listener = mock<QrFragment.OnScanCompleteListener>()
        val reader = mock<MultiFormatReader>()
        val task = QrFragment.AsyncScanningTask(listener, reader)

        val bitmap = mock<BinaryBitmap>()
        whenever(reader.decodeWithState(any())).thenThrow(NotFoundException::class.java)

        QrFragment.qrState = QrFragment.STATE_DECODE_PROGRESS
        task.processImage(bitmap)

        assertEquals(QrFragment.STATE_FIND_QRCODE, QrFragment.qrState)
    }

    @Test
    fun `async scanning task does nothing if decoding not in progress`() {
        val listener = mock<QrFragment.OnScanCompleteListener>()
        val reader = mock<MultiFormatReader>()
        val task = QrFragment.AsyncScanningTask(listener, reader)

        val bitmap = mock<BinaryBitmap>()
        whenever(reader.decodeWithState(any())).thenThrow(NotFoundException::class.java)

        QrFragment.qrState = QrFragment.STATE_FIND_QRCODE
        assertNull(task.processImage(bitmap))
        verify(reader, never()).decodeWithState(any())
    }

    @Test
    fun `async scanning decodes original unmodified image`() {
        val listener = mock<QrFragment.OnScanCompleteListener>()
        val reader = mock<MultiFormatReader>()
        val task = QrFragment.AsyncScanningTask(listener, reader)
        val imageCaptor = argumentCaptor<BinaryBitmap>()

        val bitmap = mock<BinaryBitmap>()

        QrFragment.qrState = QrFragment.STATE_DECODE_PROGRESS
        task.processImage(bitmap)
        verify(reader).decodeWithState(imageCaptor.capture())
        assertSame(bitmap, imageCaptor.value)
    }

    @Test
    fun `camera is closed on disconnect and error`() {
        val qrFragment = spy(QrFragment.newInstance(mock()))

        var camera: CameraDevice = mock()
        qrFragment.stateCallback.onDisconnected(camera)
        verify(camera).close()

        camera = mock()
        qrFragment.stateCallback.onError(camera, 0)
        verify(camera).close()
    }

    @Test
    fun `catches and handles CameraAccessException when creating preview session`() {
        val qrFragment = spy(QrFragment.newInstance(mock()))

        val camera: CameraDevice = mock()
        whenever(camera.createCaptureRequest(anyInt())).thenThrow(CameraAccessException(123))
        qrFragment.cameraDevice = camera

        val textureView: AutoFitTextureView = mock()
        whenever(textureView.surfaceTexture).thenReturn(mock())
        qrFragment.textureView = textureView

        qrFragment.previewSize = mock()

        try {
            qrFragment.createCameraPreviewSession()
        } catch (e: CameraAccessException) {
            fail("CameraAccessException should have been caught and logged, not re-thrown.")
        }
    }

    @Test
    fun `catches and handles IllegalStateException when creating preview session`() {
        val qrFragment = spy(QrFragment.newInstance(mock()))

        val camera: CameraDevice = mock()
        whenever(camera.createCaptureRequest(anyInt())).thenThrow(IllegalStateException("CameraDevice was already closed"))
        qrFragment.cameraDevice = camera

        val textureView: AutoFitTextureView = mock()
        whenever(textureView.surfaceTexture).thenReturn(mock())
        qrFragment.textureView = textureView

        qrFragment.previewSize = mock()

        try {
            qrFragment.createCameraPreviewSession()
        } catch (e: IllegalStateException) {
            fail("IllegalStateException should have been caught and logged, not re-thrown.")
        }
    }

    @Test
    fun `catches and handles CameraAccessException when opening camera`() {
        val qrFragment = spy(QrFragment.newInstance(mock()))
        whenever(qrFragment.setUpCameraOutputs(anyInt(), anyInt())).then { }

        val cameraManager: CameraManager = mock()
        whenever(cameraManager.openCamera(anyString(), any<CameraDevice.StateCallback>(), any()))
                .thenThrow(CameraAccessException(123))

        val activity: FragmentActivity = mock()
        whenever(activity.getSystemService(Context.CAMERA_SERVICE)).thenReturn(cameraManager)
        whenever(qrFragment.activity).thenReturn(activity)
        qrFragment.cameraId = "mockCamera"

        try {
            qrFragment.openCamera(1920, 1080)
        } catch (e: CameraAccessException) {
            fail("CameraAccessException should have been caught and logged, not re-thrown.")
        }
    }

    @Test
    fun `throws exception on device without camera`() {
        val qrFragment = spy(QrFragment.newInstance(mock()))
        whenever(qrFragment.setUpCameraOutputs(anyInt(), anyInt())).then { }

        val cameraManager: CameraManager = mock()
        val activity: FragmentActivity = mock()
        whenever(activity.getSystemService(Context.CAMERA_SERVICE)).thenReturn(cameraManager)
        whenever(qrFragment.activity).thenReturn(activity)

        qrFragment.cameraId = null
        try {
            qrFragment.openCamera(1920, 1080)
            fail("Expected IllegalStateException")
        } catch (e: IllegalStateException) {
            assertEquals("No camera found on device", e.message)
        }
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

    @Test
    fun `read image source adjusts for rowstride`() {
        val image: Image = mock()
        val plane: Image.Plane = mock()

        `when`(image.height).thenReturn(1080)
        `when`(image.width).thenReturn(1920)
        `when`(plane.pixelStride).thenReturn(1)
        `when`(image.planes).thenReturn(arrayOf(plane))

        // Create an image source where rowstride is equal to the width
        val bytesWithEqualRowStride: ByteBuffer = ByteBuffer.allocate(1080 * 1920)
        `when`(plane.buffer).thenReturn(bytesWithEqualRowStride)
        `when`(plane.rowStride).thenReturn(1920)
        assertArrayEquals(bytesWithEqualRowStride.array(), QrFragment.readImageSource(image).matrix)

        // Create an image source where rowstride is greater than the width
        val bytesWithNotEqualRowStride: ByteBuffer = ByteBuffer.allocate(1080 * (1920 + 128))
        `when`(plane.buffer).thenReturn(bytesWithNotEqualRowStride)
        `when`(plane.rowStride).thenReturn(2048)

        // The rowstride / offset should have been taken into account resulting
        // in the same 1080 * 1920 image source as if the rowstride was equal to the width
        assertArrayEquals(bytesWithEqualRowStride.array(), QrFragment.readImageSource(image).matrix)
    }
}