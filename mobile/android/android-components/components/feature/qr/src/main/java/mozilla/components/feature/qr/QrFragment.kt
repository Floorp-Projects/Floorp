/*
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

package mozilla.components.feature.qr

import android.annotation.SuppressLint
import android.content.Context
import android.content.res.Configuration
import android.graphics.ImageFormat
import android.graphics.Matrix
import android.graphics.Point
import android.graphics.RectF
import android.graphics.SurfaceTexture
import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.media.Image
import android.media.ImageReader
import android.os.AsyncTask
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.util.Size
import android.view.LayoutInflater
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.zxing.BinaryBitmap
import com.google.zxing.MultiFormatReader
import com.google.zxing.NotFoundException
import com.google.zxing.PlanarYUVLuminanceSource
import com.google.zxing.common.HybridBinarizer
import mozilla.components.support.base.android.view.AutoFitTextureView
import mozilla.components.support.base.log.logger.Logger
import java.io.Serializable
import java.util.ArrayList
import java.util.Arrays
import java.util.Collections
import java.util.Comparator
import java.util.concurrent.Semaphore
import java.util.concurrent.TimeUnit
import kotlin.math.max
import kotlin.math.min

/**
 * A [Fragment] that displays a QR scanner.
 *
 * This class is based on Camera2BasicFragment from:
 *
 * https://github.com/googlesamples/android-Camera2Basic
 * https://github.com/kismkof/camera2basic
 */
@Suppress("TooManyFunctions", "LargeClass")
class QrFragment : Fragment() {
    private val logger = Logger("mozac-qr")

    /**
     * [TextureView.SurfaceTextureListener] handles several lifecycle events on a [TextureView].
     */
    private val surfaceTextureListener = object : TextureView.SurfaceTextureListener {

        override fun onSurfaceTextureAvailable(texture: SurfaceTexture, width: Int, height: Int) {
            openCamera(width, height)
        }

        override fun onSurfaceTextureSizeChanged(texture: SurfaceTexture, width: Int, height: Int) {
            configureTransform(width, height)
        }

        @Suppress("EmptyFunctionBlock")
        override fun onSurfaceTextureUpdated(texture: SurfaceTexture) { }

        override fun onSurfaceTextureDestroyed(texture: SurfaceTexture): Boolean {
            return true
        }
    }

    internal lateinit var textureView: AutoFitTextureView
    internal var cameraId: String? = null
    private var captureSession: CameraCaptureSession? = null
    internal var cameraDevice: CameraDevice? = null
    internal var previewSize: Size? = null

    /**
     * Listener invoked when the QR scan completed successfully.
     */
    interface OnScanCompleteListener : Serializable {
        /**
         * Invoked to provide access to the result of the QR scan.
         */
        fun onScanComplete(result: String)
    }

    @Volatile internal var scanCompleteListener: OnScanCompleteListener? = null
        set(value) {
            field = object : OnScanCompleteListener {
                override fun onScanComplete(result: String) {
                    Handler(Looper.getMainLooper()).apply {
                        post {
                            value?.onScanComplete(result)
                        }
                    }
                }
            }
        }

    /**
     * [CameraDevice.StateCallback] is called when [CameraDevice] changes its state.
     */
    internal val stateCallback = object : CameraDevice.StateCallback() {

        override fun onOpened(cameraDevice: CameraDevice) {
            cameraOpenCloseLock.release()
            this@QrFragment.cameraDevice = cameraDevice
            createCameraPreviewSession()
        }

        override fun onDisconnected(cameraDevice: CameraDevice) {
            cameraOpenCloseLock.release()
            cameraDevice.close()
            this@QrFragment.cameraDevice = null
        }

        override fun onError(cameraDevice: CameraDevice, error: Int) {
            cameraOpenCloseLock.release()
            cameraDevice.close()
            this@QrFragment.cameraDevice = null
        }
    }

    /**
     * An additional thread for running tasks that shouldn't block the UI.
     * A [Handler] for running tasks in the background.
     */
    private var backgroundThread: HandlerThread? = null
    private var backgroundHandler: Handler? = null
    private var previewRequestBuilder: CaptureRequest.Builder? = null
    private var previewRequest: CaptureRequest? = null

    /**
     * A [Semaphore] to prevent the app from exiting before closing the camera.
     */
    private val cameraOpenCloseLock = Semaphore(1)

    /**
     * Orientation of the camera sensor
     */
    private var sensorOrientation: Int = 0

    /**
     * An [ImageReader] that handles still image capture.
     * This is the output file for our picture.
     */
    private var imageReader: ImageReader? = null
    private val imageAvailableListener = object : ImageReader.OnImageAvailableListener {

        private var image: Image? = null

        override fun onImageAvailable(reader: ImageReader) {
            try {
                image = reader.acquireNextImage()
                val availableImage = image
                if (availableImage != null) {
                    val source = readImageSource(availableImage)
                    val bitmap = BinaryBitmap(HybridBinarizer(source))
                    if (qrState == STATE_FIND_QRCODE) {
                        qrState = STATE_DECODE_PROGRESS
                        AsyncScanningTask(scanCompleteListener).execute(bitmap)
                    }
                }
            } finally {
                image?.close()
            }
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_layout, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        textureView = view.findViewById<View>(R.id.texture) as AutoFitTextureView
        qrState = STATE_FIND_QRCODE
    }

    override fun onResume() {
        super.onResume()
        startBackgroundThread()
        // When the screen is turned off and turned back on, the SurfaceTexture is already
        // available, and "onSurfaceTextureAvailable" will not be called. In that case, we can open
        // a camera and start preview from here (otherwise, we wait until the surface is ready in
        // the SurfaceTextureListener).
        if (textureView.isAvailable) {
            openCamera(textureView.width, textureView.height)
        } else {
            textureView.surfaceTextureListener = surfaceTextureListener
        }
    }

    override fun onPause() {
        closeCamera()
        stopBackgroundThread()
        super.onPause()
    }

    internal fun startBackgroundThread() {
        backgroundThread = HandlerThread("CameraBackground").apply {
            start()
            backgroundHandler = Handler(this.looper)
        }
    }

    internal fun stopBackgroundThread() {
        backgroundThread?.quitSafely()
        try {
            backgroundThread?.join()
            backgroundThread = null
            backgroundHandler = null
        } catch (e: InterruptedException) {
            logger.debug("Interrupted while stopping background thread", e)
        }
    }

    /**
     * Sets up member variables related to camera.
     *
     * @param width The width of available size for camera preview
     * @param height The height of available size for camera preview
     */
    @Suppress("ComplexMethod")
    internal fun setUpCameraOutputs(width: Int, height: Int) {
        val manager = activity?.getSystemService(Context.CAMERA_SERVICE) as CameraManager? ?: return

        for (cameraId in manager.cameraIdList) {
            val characteristics = manager.getCameraCharacteristics(cameraId)

            val facing = characteristics.get(CameraCharacteristics.LENS_FACING)
            if (facing == CameraCharacteristics.LENS_FACING_FRONT) {
                continue
            }

            val map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP) ?: continue
            val largest = Collections.max(map.getOutputSizes(ImageFormat.YUV_420_888).asList(), CompareSizesByArea())
            imageReader = ImageReader.newInstance(MAX_PREVIEW_WIDTH, MAX_PREVIEW_HEIGHT, ImageFormat.YUV_420_888, 2)
                    .apply { setOnImageAvailableListener(imageAvailableListener, backgroundHandler) }

            // Find out if we need to swap dimension to get the preview size relative to sensor coordinate.
            val displayRotation = activity?.windowManager?.defaultDisplay?.rotation

            sensorOrientation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION) as Int

            @Suppress("MagicNumber")
            val swappedDimensions = when (displayRotation) {
                Surface.ROTATION_0, Surface.ROTATION_180 -> sensorOrientation == 90 || sensorOrientation == 270
                Surface.ROTATION_90, Surface.ROTATION_270 -> sensorOrientation == 0 || sensorOrientation == 180
                else -> false
            }

            val displaySize = Point()
            activity?.windowManager?.defaultDisplay?.getSize(displaySize)
            var rotatedPreviewWidth = width
            var rotatedPreviewHeight = height
            var maxPreviewWidth = displaySize.x
            var maxPreviewHeight = displaySize.y

            if (swappedDimensions) {
                rotatedPreviewWidth = height
                rotatedPreviewHeight = width
                maxPreviewWidth = displaySize.y
                maxPreviewHeight = displaySize.x
            }

            maxPreviewWidth = min(maxPreviewWidth, MAX_PREVIEW_WIDTH)
            maxPreviewHeight = min(maxPreviewHeight, MAX_PREVIEW_HEIGHT)

            previewSize = chooseOptimalSize(map.getOutputSizes(SurfaceTexture::class.java),
                    rotatedPreviewWidth, rotatedPreviewHeight, maxPreviewWidth,
                    maxPreviewHeight, largest)

            val size = previewSize as Size
            // We fit the aspect ratio of TextureView to the size of preview we picked.
            val orientation = resources.configuration.orientation
            if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
                textureView.setAspectRatio(size.width, size.height)
            } else {
                textureView.setAspectRatio(size.height, size.width)
            }

            this.cameraId = cameraId
            return
        }
    }

    /**
     * Opens the camera specified by [QrFragment.cameraId].
     */
    @SuppressLint("MissingPermission")
    @Suppress("ThrowsCount")
    internal fun openCamera(width: Int, height: Int) {
        try {
            setUpCameraOutputs(width, height)
            if (cameraId == null) {
                throw IllegalStateException("No camera found on device")
            }

            configureTransform(width, height)

            val activity = activity
            val manager = activity?.getSystemService(Context.CAMERA_SERVICE) as CameraManager?

            if (!cameraOpenCloseLock.tryAcquire(CAMERA_CLOSE_LOCK_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw IllegalStateException("Time out waiting to lock camera opening.")
            }
            manager?.openCamera(cameraId as String, stateCallback, backgroundHandler)
        } catch (e: InterruptedException) {
            throw IllegalStateException("Interrupted while trying to lock camera opening.", e)
        } catch (e: CameraAccessException) {
            logger.error("Failed to open camera", e)
        }
    }

    /**
     * Closes the current [CameraDevice].
     */
    internal fun closeCamera() {
        try {
            cameraOpenCloseLock.acquire()
            captureSession?.close()
            captureSession = null
            cameraDevice?.close()
            cameraDevice = null
            imageReader?.close()
            imageReader = null
        } catch (e: InterruptedException) {
            throw IllegalStateException("Interrupted while trying to lock camera closing.", e)
        } finally {
            cameraOpenCloseLock.release()
        }
    }

    /**
     * Configures the necessary [android.graphics.Matrix] transformation to `textureView`.
     * This method should be called after the camera preview size is determined in
     * [setUpCameraOutputs] and also the size of `textureView` is fixed.
     *
     * @param viewWidth The width of `textureView`
     * @param viewHeight The height of `textureView`
     */
    private fun configureTransform(viewWidth: Int, viewHeight: Int) {
        val activity = activity ?: return
        val size = previewSize ?: return
        val rotation = activity.windowManager.defaultDisplay.rotation
        val matrix = Matrix()
        val viewRect = RectF(0f, 0f, viewWidth.toFloat(), viewHeight.toFloat())
        val bufferRect = RectF(0f, 0f, size.height.toFloat(), size.width.toFloat())
        val centerX = viewRect.centerX()
        val centerY = viewRect.centerY()

        @Suppress("MagicNumber")
        if (Surface.ROTATION_90 == rotation || Surface.ROTATION_270 == rotation) {
            bufferRect.offset(centerX - bufferRect.centerX(), centerY - bufferRect.centerY())
            matrix.setRectToRect(viewRect, bufferRect, Matrix.ScaleToFit.FILL)
            val scale = max(viewHeight.toFloat() / size.height, viewWidth.toFloat() / size.width)
            matrix.postScale(scale, scale, centerX, centerY)
            matrix.postRotate((90 * (rotation - 2)).toFloat(), centerX, centerY)
        } else if (Surface.ROTATION_180 == rotation) {
            matrix.postRotate(180f, centerX, centerY)
        }
        textureView.setTransform(matrix)
    }

    /**
     * Creates a new [CameraCaptureSession] for camera preview.
     */
    @Suppress("ComplexMethod")
    internal fun createCameraPreviewSession() {
        val texture = textureView.surfaceTexture

        val size = previewSize as Size
        // We configure the size of default buffer to be the size of camera preview we want.
        texture.setDefaultBufferSize(size.width, size.height)

        val surface = Surface(texture)
        val mImageSurface = imageReader?.surface

        handleCaptureException("Failed to create camera preview session") {
            cameraDevice?.let {
                previewRequestBuilder = it.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW).apply {
                    addTarget(mImageSurface as Surface)
                    addTarget(surface)
                }

                val captureCallback = object : CameraCaptureSession.CaptureCallback() {}
                val stateCallback = object : CameraCaptureSession.StateCallback() {
                    override fun onConfigured(cameraCaptureSession: CameraCaptureSession) {
                        if (null == cameraDevice) return

                        previewRequestBuilder?.set(CaptureRequest.CONTROL_AF_MODE,
                                CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)

                        previewRequest = previewRequestBuilder?.build()
                        captureSession = cameraCaptureSession

                        handleCaptureException("Failed to request capture") {
                            cameraCaptureSession.setRepeatingRequest(
                                previewRequest as CaptureRequest,
                                captureCallback,
                                backgroundHandler
                            )
                        }
                    }

                    override fun onConfigureFailed(cameraCaptureSession: CameraCaptureSession) {
                        logger.error("Failed to configure CameraCaptureSession")
                    }
                }

                it.createCaptureSession(Arrays.asList(mImageSurface, surface), stateCallback, null)
            }
        }
    }

    @Suppress("TooGenericExceptionCaught")
    private fun handleCaptureException(msg: String, block: () -> Unit) {
        try {
            block()
        } catch (e: Exception) {
            when (e) {
                is CameraAccessException, is IllegalStateException -> {
                    logger.error(msg, e)
                }
                else -> throw e
            }
        }
    }

    /**
     * Compares two `Size`s based on their areas.
     */
    internal class CompareSizesByArea : Comparator<Size> {
        override fun compare(lhs: Size, rhs: Size): Int {
            return java.lang.Long.signum(lhs.width.toLong() * lhs.height - rhs.width.toLong() * rhs.height)
        }
    }

    companion object {
        internal const val STATE_FIND_QRCODE = 0
        internal const val STATE_DECODE_PROGRESS = 1
        internal const val STATE_QRCODE_EXIST = 2

        internal const val MAX_PREVIEW_WIDTH = 1920
        internal const val MAX_PREVIEW_HEIGHT = 1080

        private const val CAMERA_CLOSE_LOCK_TIMEOUT_MS = 2500L

        fun newInstance(listener: OnScanCompleteListener): QrFragment {
            return QrFragment().apply {
                scanCompleteListener = listener
            }
        }

        /**
         * Given `choices` of `Size`s supported by a camera, choose the smallest one that
         * is at least as large as the respective texture view size, and that is at most as large as the
         * respective max size, and whose aspect ratio matches with the specified value. If such size
         * doesn't exist, choose the largest one that is at most as large as the respective max size,
         * and whose aspect ratio matches with the specified value.
         *
         * @param choices The list of sizes that the camera supports for the intended output class
         * @param textureViewWidth The width of the texture view relative to sensor coordinate
         * @param textureViewHeight The height of the texture view relative to sensor coordinate
         * @param maxWidth The maximum width that can be chosen
         * @param maxHeight The maximum height that can be chosen
         * @param aspectRatio The aspect ratio
         * @return The optimal `Size`, or an arbitrary one if none were big enough.
         */
        @Suppress("LongParameterList")
        internal fun chooseOptimalSize(
            choices: Array<Size>,
            textureViewWidth: Int,
            textureViewHeight: Int,
            maxWidth: Int,
            maxHeight: Int,
            aspectRatio: Size
        ): Size {

            // Collect the supported resolutions that are at least as big as the preview Surface
            val bigEnough = ArrayList<Size>()
            // Collect the supported resolutions that are smaller than the preview Surface
            val notBigEnough = ArrayList<Size>()
            val w = aspectRatio.width
            val h = aspectRatio.height
            for (option in choices) {
                if (option.width <= maxWidth && option.height <= maxHeight &&
                        option.height == option.width * h / w) {
                    if (option.width >= textureViewWidth && option.height >= textureViewHeight) {
                        bigEnough.add(option)
                    } else {
                        notBigEnough.add(option)
                    }
                }
            }

            // Pick the smallest of those big enough. If there is no one big enough, pick the
            // largest of those not big enough.
            return when {
                bigEnough.size > 0 -> Collections.min(bigEnough, CompareSizesByArea())
                notBigEnough.size > 0 -> Collections.max(notBigEnough, CompareSizesByArea())
                else -> choices[0]
            }
        }

        internal fun readImageSource(image: Image): PlanarYUVLuminanceSource {
            val plane = image.planes[0]
            val buffer = plane.buffer
            val data = ByteArray(buffer.remaining()).also { buffer.get(it) }

            val height = image.height
            val width = image.width
            val dataWidth = width + ((plane.rowStride - plane.pixelStride * width) / plane.pixelStride)
            return PlanarYUVLuminanceSource(data, dataWidth, height, 0, 0, width, height, false)
        }

        @Volatile internal var qrState: Int = 0
    }

    internal class AsyncScanningTask(
        private val scanCompleteListener: OnScanCompleteListener?,
        private val multiFormatReader: MultiFormatReader = MultiFormatReader()
    ) : AsyncTask<BinaryBitmap, Void, Void>() {

        override fun doInBackground(vararg bitmaps: BinaryBitmap): Void? {
            return processImage(bitmaps[0])
        }

        fun processImage(bitmap: BinaryBitmap): Void? {
            if (qrState != STATE_DECODE_PROGRESS) {
                return null
            }

            try {
                val rawResult = multiFormatReader.decodeWithState(bitmap)
                if (rawResult != null) {
                    qrState = STATE_QRCODE_EXIST
                    scanCompleteListener?.onScanComplete(rawResult.toString())
                }
            } catch (e: NotFoundException) {
                qrState = STATE_FIND_QRCODE
            } finally {
                multiFormatReader.reset()
            }

            return null
        }
    }
}
