/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import android.content.Context
import android.content.Intent.ACTION_GET_CONTENT
import android.content.Intent.CATEGORY_OPENABLE
import android.content.Intent.EXTRA_ALLOW_MULTIPLE
import android.content.Intent.EXTRA_MIME_TYPES
import android.content.pm.ActivityInfo
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.content.pm.PackageManager.MATCH_DEFAULT_ONLY
import android.content.pm.ProviderInfo
import android.content.pm.ResolveInfo
import android.net.Uri
import android.provider.MediaStore.ACTION_IMAGE_CAPTURE
import android.provider.MediaStore.ACTION_VIDEO_CAPTURE
import android.provider.MediaStore.Audio.Media.RECORD_SOUND_ACTION
import android.provider.MediaStore.EXTRA_OUTPUT
import android.webkit.MimeTypeMap
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.eq
import org.mockito.ArgumentMatchers.notNull
import org.mockito.Mockito.mock
import org.mockito.Mockito.`when`
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class MimeTypeTest {

    private val request = PromptRequest.File(
        mimeTypes = emptyArray(),
        onSingleFileSelected = { _, _ -> },
        onMultipleFilesSelected = { _, _ -> },
        onDismiss = {},
    )
    private val capture = PromptRequest.File.FacingMode.ANY

    private lateinit var context: Context
    private lateinit var packageManager: PackageManager

    @Before
    fun setup() {
        context = mock(Context::class.java)
        packageManager = mock(PackageManager::class.java)

        `when`(context.packageManager).thenReturn(packageManager)
        `when`(context.packageName).thenReturn("org.mozilla.browser")
        `when`(packageManager.resolveActivity(notNull(), eq(MATCH_DEFAULT_ONLY))).thenReturn(null)
    }

    @Test
    fun `matches empty list of mime types`() {
        assertTypes(setOf(MimeType.Wildcard)) {
            it.matches(emptyArray())
        }
    }

    @Test
    fun `matches varied list of mime types`() {
        assertTypes(setOf(MimeType.Image(), MimeType.Audio, MimeType.Wildcard)) {
            it.matches(arrayOf("image/*", "audio/*"))
        }
        assertTypes(setOf(MimeType.Audio, MimeType.Video, MimeType.Wildcard)) {
            it.matches(arrayOf("video/mp4", "audio/*"))
        }
    }

    @Test
    fun `matches image types`() {
        assertTypes(setOf(MimeType.Image(), MimeType.Wildcard)) {
            it.matches(arrayOf("image/*"))
        }
        assertTypes(setOf(MimeType.Image(), MimeType.Wildcard)) {
            it.matches(arrayOf("image/jpg"))
        }
        assertTypes(setOf(MimeType.Wildcard)) {
            it.matches(arrayOf(".webp"))
        }
        assertTypes(setOf(MimeType.Image(), MimeType.Wildcard)) {
            it.matches(arrayOf(".jpg", "image/*", ".gif"))
        }
    }

    @Test
    fun `matches video types`() {
        assertTypes(setOf(MimeType.Video, MimeType.Wildcard)) {
            it.matches(arrayOf("video/*"))
        }
        assertTypes(setOf(MimeType.Video, MimeType.Wildcard)) {
            it.matches(arrayOf("video/avi"))
        }
        assertTypes(setOf(MimeType.Wildcard)) {
            it.matches(arrayOf(".webm"))
        }
        assertTypes(setOf(MimeType.Video, MimeType.Wildcard)) {
            it.matches(arrayOf("video/*", ".mov", ".mp4"))
        }
    }

    @Test
    fun `matches audio types`() {
        assertTypes(setOf(MimeType.Audio, MimeType.Wildcard)) {
            it.matches(arrayOf("audio/*"))
        }
        assertTypes(setOf(MimeType.Audio, MimeType.Wildcard)) {
            it.matches(arrayOf("audio/wav"))
        }
        assertTypes(setOf(MimeType.Wildcard)) {
            it.matches(arrayOf(".mp3"))
        }
        assertTypes(setOf(MimeType.Audio, MimeType.Wildcard)) {
            it.matches(arrayOf(".ogg", "audio/wav", "audio/*"))
        }
    }

    @Test
    fun `matches document types`() {
        assertTypes(setOf(MimeType.Wildcard)) {
            it.matches(arrayOf("application/json"))
        }
        assertTypes(setOf(MimeType.Wildcard)) {
            it.matches(arrayOf(".doc"))
        }
        assertTypes(setOf(MimeType.Wildcard)) {
            it.matches(arrayOf(".txt", "text/html"))
        }
    }

    @Test
    fun `shouldCapture empty list of mime types`() {
        assertTypes(setOf()) {
            it.shouldCapture(emptyArray(), capture)
        }
    }

    @Test
    fun `shouldCapture varied list of mime types`() {
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf("image/*", "video/*"), capture)
        }
    }

    @Test
    fun `shouldCapture image types`() {
        assertTypes(setOf(MimeType.Image())) {
            it.shouldCapture(arrayOf("image/*"), capture)
        }
        assertTypes(setOf(MimeType.Image())) {
            it.shouldCapture(arrayOf("image/jpg"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf(".webp"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf(".jpg", "image/*", ".gif"), capture)
        }
        assertTypes(setOf(MimeType.Image())) {
            it.shouldCapture(arrayOf("image/png", "image/jpg"), capture)
        }
    }

    @Test
    fun `shouldCapture video types`() {
        assertTypes(setOf(MimeType.Video)) {
            it.shouldCapture(arrayOf("video/*"), capture)
        }
        assertTypes(setOf(MimeType.Video)) {
            it.shouldCapture(arrayOf("video/avi"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf(".webm"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf("video/*", ".mov", ".mp4"), capture)
        }
        assertTypes(setOf(MimeType.Video)) {
            it.shouldCapture(arrayOf("video/webm", "video/*"), capture)
        }
    }

    @Test
    fun `shouldCapture audio types`() {
        assertTypes(setOf(MimeType.Audio)) {
            it.shouldCapture(arrayOf("audio/*"), capture)
        }
        assertTypes(setOf(MimeType.Audio)) {
            it.shouldCapture(arrayOf("audio/wav"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf(".mp3"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf(".ogg", "audio/wav", "audio/*"), capture)
        }
        assertTypes(setOf(MimeType.Audio)) {
            it.shouldCapture(arrayOf("audio/wav", "audio/ogg"), capture)
        }
    }

    @Test
    fun `shouldCapture document types`() {
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf("application/json"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf(".doc"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf(".txt", "text/html"), capture)
        }
        assertTypes(setOf()) {
            it.shouldCapture(arrayOf("text/plain", "text/html"), capture)
        }
    }

    @Test
    fun `Image buildIntent`() {
        assertNull(MimeType.Image().buildIntent(context, request))

        val uri = Uri.parse("context://abcd")
        val image = MimeType.Image { _, _, _ -> uri }

        `when`(
            packageManager
                .resolveContentProvider(eq("org.mozilla.browser.fileprovider"), anyInt()),
        )
            .thenReturn(mock(ProviderInfo::class.java))
        mockResolveActivity()

        image.buildIntent(context, request)?.run {
            assertEquals(action, ACTION_IMAGE_CAPTURE)
            assertEquals(1, extras?.size())

            val photoUri = extras!!.get(EXTRA_OUTPUT) as Uri
            assertEquals(uri, photoUri)
        }

        val anyCaptureRequest = request.copy(captureMode = PromptRequest.File.FacingMode.ANY)
        image.buildIntent(context, anyCaptureRequest)?.run {
            assertEquals(action, ACTION_IMAGE_CAPTURE)
            assertEquals(1, extras?.size())
        }

        val frontCaptureRequest = request.copy(captureMode = PromptRequest.File.FacingMode.FRONT_CAMERA)
        image.buildIntent(context, frontCaptureRequest)?.run {
            assertEquals(action, ACTION_IMAGE_CAPTURE)
            assertEquals(1, extras!!.get(MimeType.CAMERA_FACING))
            assertEquals(1, extras!!.get(MimeType.LENS_FACING_FRONT))
            assertEquals(true, extras!!.get(MimeType.USE_FRONT_CAMERA))
        }

        val backCaptureRequest = request.copy(captureMode = PromptRequest.File.FacingMode.BACK_CAMERA)
        image.buildIntent(context, backCaptureRequest)?.run {
            assertEquals(action, ACTION_IMAGE_CAPTURE)
            assertEquals(0, extras!!.get(MimeType.CAMERA_FACING))
            assertEquals(1, extras!!.get(MimeType.LENS_FACING_BACK))
            assertEquals(true, extras!!.get(MimeType.USE_BACK_CAMERA))
        }
    }

    @Test
    fun `Video buildIntent`() {
        assertNull(MimeType.Video.buildIntent(context, request))

        mockResolveActivity()
        MimeType.Video.buildIntent(context, request)?.run {
            assertEquals(action, ACTION_VIDEO_CAPTURE)
            assertNull(extras)
        }

        val anyCaptureRequest = request.copy(captureMode = PromptRequest.File.FacingMode.ANY)
        MimeType.Video.buildIntent(context, anyCaptureRequest)?.run {
            assertEquals(action, ACTION_VIDEO_CAPTURE)
            assertNull(extras)
        }

        val frontCaptureRequest = request.copy(captureMode = PromptRequest.File.FacingMode.FRONT_CAMERA)
        MimeType.Video.buildIntent(context, frontCaptureRequest)?.run {
            assertEquals(action, ACTION_VIDEO_CAPTURE)
            assertEquals(1, extras!!.get(MimeType.CAMERA_FACING))
            assertEquals(1, extras!!.get(MimeType.LENS_FACING_FRONT))
            assertEquals(true, extras!!.get(MimeType.USE_FRONT_CAMERA))
        }

        val backCaptureRequest = request.copy(captureMode = PromptRequest.File.FacingMode.BACK_CAMERA)
        MimeType.Video.buildIntent(context, backCaptureRequest)?.run {
            assertEquals(action, ACTION_VIDEO_CAPTURE)
            assertEquals(0, extras!!.get(MimeType.CAMERA_FACING))
            assertEquals(1, extras!!.get(MimeType.LENS_FACING_BACK))
            assertEquals(true, extras!!.get(MimeType.USE_BACK_CAMERA))
        }
    }

    @Test
    fun `Audio buildIntent`() {
        assertNull(MimeType.Audio.buildIntent(context, request))

        mockResolveActivity()
        MimeType.Audio.buildIntent(context, request)?.run {
            assertEquals(action, RECORD_SOUND_ACTION)
        }
    }

    @Test
    fun `Wildcard buildIntent`() {
        // allowMultipleFiles false and empty mimeTypes will create an intent
        // without EXTRA_ALLOW_MULTIPLE and EXTRA_MIME_TYPES
        with(MimeType.Wildcard.buildIntent(testContext, request)) {
            assertEquals(action, ACTION_GET_CONTENT)
            assertEquals(type, "*/*")
            assertTrue(categories.contains(CATEGORY_OPENABLE))

            val mimeType = extras!!.get(EXTRA_MIME_TYPES)
            assertNull(mimeType)

            val allowMultipleFiles = extras!!.getBoolean(EXTRA_ALLOW_MULTIPLE)
            assertFalse(allowMultipleFiles)
        }

        // allowMultipleFiles true and not empty mimeTypes will create an intent
        // with EXTRA_ALLOW_MULTIPLE and EXTRA_MIME_TYPES
        val multiJpegRequest = request.copy(
            mimeTypes = arrayOf("image/jpeg"),
            isMultipleFilesSelection = true,
        )
        with(MimeType.Wildcard.buildIntent(testContext, multiJpegRequest)) {
            assertEquals(action, ACTION_GET_CONTENT)
            assertEquals(type, "*/*")
            assertTrue(categories.contains(CATEGORY_OPENABLE))

            val mimeTypes = extras!!.get(EXTRA_MIME_TYPES) as Array<*>
            assertEquals(mimeTypes.first(), "image/jpeg")

            val allowMultipleFiles = extras!!.getBoolean(EXTRA_ALLOW_MULTIPLE)
            assertTrue(allowMultipleFiles)
        }
    }

    @Test
    fun `Wildcard buildIntent with file extensions`() {
        shadowOf(MimeTypeMap.getSingleton()).apply {
            addExtensionMimeTypMapping(".gif", "image/gif")
            addExtensionMimeTypMapping(
                "docx",
                "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
            )
        }

        val extensionsRequest = request.copy(mimeTypes = arrayOf(".gif", "image/jpeg", "docx", ".fun"))

        with(MimeType.Wildcard.buildIntent(testContext, extensionsRequest)) {
            assertEquals(action, ACTION_GET_CONTENT)

            val mimeTypes = extras!!.get(EXTRA_MIME_TYPES) as Array<*>
            assertEquals(mimeTypes[0], "image/gif")
            assertEquals(mimeTypes[1], "image/jpeg")
            assertEquals(mimeTypes[2], "application/vnd.openxmlformats-officedocument.wordprocessingml.document")
            assertEquals(mimeTypes[3], "*/*")
        }
    }

    private fun mockResolveActivity() {
        val info = ResolveInfo()
        info.activityInfo = ActivityInfo()
        info.activityInfo.applicationInfo = ApplicationInfo()
        info.activityInfo.applicationInfo.packageName = "com.example.app"
        info.activityInfo.name = "SomeActivity"
        `when`(packageManager.resolveActivity(notNull(), eq(MATCH_DEFAULT_ONLY))).thenReturn(info)
    }

    private fun assertTypes(valid: Set<MimeType>, func: (MimeType) -> Boolean) {
        assertEquals(valid, MimeType.values().filter(func).toSet())
    }
}
