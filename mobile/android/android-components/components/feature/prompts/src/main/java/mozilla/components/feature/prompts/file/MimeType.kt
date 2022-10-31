/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import android.Manifest.permission.CAMERA
import android.Manifest.permission.READ_EXTERNAL_STORAGE
import android.Manifest.permission.RECORD_AUDIO
import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_GET_CONTENT
import android.content.Intent.CATEGORY_OPENABLE
import android.content.Intent.EXTRA_ALLOW_MULTIPLE
import android.content.Intent.EXTRA_LOCAL_ONLY
import android.content.Intent.EXTRA_MIME_TYPES
import android.net.Uri
import android.provider.MediaStore.ACTION_IMAGE_CAPTURE
import android.provider.MediaStore.ACTION_VIDEO_CAPTURE
import android.provider.MediaStore.Audio.Media.RECORD_SOUND_ACTION
import android.provider.MediaStore.EXTRA_OUTPUT
import android.webkit.MimeTypeMap
import androidx.core.content.FileProvider.getUriForFile
import mozilla.components.concept.engine.prompt.PromptRequest.File
import java.io.IOException
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale.US

internal sealed class MimeType(
    private val type: String,
    val permission: String,
) {

    data class Image(
        private val getUri: (Context, String, java.io.File) -> Uri = { context, authority, file ->
            getUriForFile(context, authority, file)
        },
    ) : MimeType("image/", CAMERA) {
        /**
         * Build an image capture intent using the application FileProvider.
         * A FileProvider must be defined in your AndroidManifest.xml, see
         * https://developer.android.com/training/camera/photobasics#TaskPath
         */
        override fun buildIntent(context: Context, request: File): Intent? {
            val intent = Intent(ACTION_IMAGE_CAPTURE).withDeviceSupport(context) ?: return null

            val photoFile = try {
                val filename = SimpleDateFormat("yyyy-MM-ddHH.mm.ss", US).format(Date())
                java.io.File.createTempFile(filename, ".jpg", context.cacheDir)
            } catch (e: IOException) {
                return null
            }

            val photoUri = getUri(context, "${context.packageName}.feature.prompts.fileprovider", photoFile)

            return intent.apply { putExtra(EXTRA_OUTPUT, photoUri) }.addCaptureHint(request.captureMode)
        }
    }

    object Video : MimeType("video/", CAMERA) {
        override fun buildIntent(context: Context, request: File) =
            Intent(ACTION_VIDEO_CAPTURE).withDeviceSupport(context)?.addCaptureHint(request.captureMode)
    }

    object Audio : MimeType("audio/", RECORD_AUDIO) {
        override fun buildIntent(context: Context, request: File) =
            Intent(RECORD_SOUND_ACTION).withDeviceSupport(context)
    }

    object Wildcard : MimeType("*/", READ_EXTERNAL_STORAGE) {
        private val mimeTypeMap = MimeTypeMap.getSingleton()

        override fun matches(mimeTypes: Array<out String>) = true

        override fun shouldCapture(mimeTypes: Array<out String>, capture: File.FacingMode) = false

        override fun buildIntent(context: Context, request: File) =
            Intent(ACTION_GET_CONTENT).apply {
                type = "*/*"
                addCategory(CATEGORY_OPENABLE)
                putExtra(EXTRA_LOCAL_ONLY, true)
                if (request.mimeTypes.isNotEmpty()) {
                    val types = request.mimeTypes
                        .map {
                            if (it.contains("/")) {
                                it
                            } else {
                                mimeTypeMap.getMimeTypeFromExtension(it) ?: "*/*"
                            }
                        }
                        .toTypedArray()
                    putExtra(EXTRA_MIME_TYPES, types)
                }
                putExtra(EXTRA_ALLOW_MULTIPLE, request.isMultipleFilesSelection)
            }
    }

    /**
     * True if any of the given mime values match this type. If no values are specified, then
     * there will not be a match.
     */
    open fun matches(mimeTypes: Array<out String>) =
        mimeTypes.isNotEmpty() && mimeTypes.any { it.startsWith(type) }

    open fun shouldCapture(mimeTypes: Array<out String>, capture: File.FacingMode) =
        capture != File.FacingMode.NONE &&
            mimeTypes.isNotEmpty() &&
            mimeTypes.all { it.startsWith(type) }

    abstract fun buildIntent(context: Context, request: File): Intent?

    companion object {
        /**
         * List of all MimeTypes that can be iterated
         */
        fun values() = listOf(Image(), Video, Audio, Wildcard)

        const val CAMERA_FACING = "android.intent.extras.CAMERA_FACING"
        const val LENS_FACING_FRONT = "android.intent.extras.LENS_FACING_FRONT"
        const val USE_FRONT_CAMERA = "android.intent.extra.USE_FRONT_CAMERA"
        const val LENS_FACING_BACK = "android.intent.extras.LENS_FACING_BACK"
        const val USE_BACK_CAMERA = "android.intent.extra.USE_BACK_CAMERA"
    }
}

/**
 * Return the intent only if its type has any corresponding apps on the device.
 */
@SuppressLint("QueryPermissionsNeeded") // We expect our browsers to have the QUERY_ALL_PACKAGES permission
private fun Intent.withDeviceSupport(context: Context) =
    if (resolveActivity(context.packageManager) != null) this else null

/**
 * Hacky request for specific camera orientation
 * https://stackoverflow.com/questions/43841738
 */
private fun Intent.addCaptureHint(capture: File.FacingMode): Intent? {
    if (capture == File.FacingMode.FRONT_CAMERA) {
        putExtra(MimeType.CAMERA_FACING, 1)
        putExtra(MimeType.LENS_FACING_FRONT, 1)
        putExtra(MimeType.USE_FRONT_CAMERA, true)
    } else if (capture == File.FacingMode.BACK_CAMERA) {
        putExtra(MimeType.CAMERA_FACING, 0)
        putExtra(MimeType.LENS_FACING_BACK, 1)
        putExtra(MimeType.USE_BACK_CAMERA, true)
    }
    return this
}
