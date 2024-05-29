/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import android.app.Activity
import android.app.Activity.RESULT_OK
import android.content.Context
import android.content.Intent
import android.content.Intent.EXTRA_INITIAL_INTENTS
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.net.Uri
import android.provider.MediaStore.EXTRA_OUTPUT
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.fragment.app.Fragment
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.File
import mozilla.components.feature.prompts.PromptContainer
import mozilla.components.feature.prompts.consumePromptFrom
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.isPermissionGranted
import mozilla.components.support.ktx.android.net.getFileName
import mozilla.components.support.ktx.android.net.isUnderPrivateAppDirectory
import mozilla.components.support.utils.ext.getParcelableExtraCompat

/**
 * The image capture intent doesn't return the URI where the image is saved,
 * so we track it here.
 *
 * Top-level scoped to survive activity recreation in the "Don't keep activities" scenario.
 */
@VisibleForTesting
internal var captureUri: Uri? = null

/**
 * @property container The [Activity] or [Fragment] which hosts the file picker.
 * @property store The [BrowserStore] this feature should subscribe to.
 * @property fileUploadsDirCleaner a [FileUploadsDirCleaner] to clean up temporary file uploads.
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested before a prompt (e.g. a file picker) can be displayed.
 * Once the request is completed, [onPermissionsResult] needs to be invoked.
 */
internal class FilePicker(
    private val container: PromptContainer,
    private val store: BrowserStore,
    private var sessionId: String? = null,
    private var fileUploadsDirCleaner: FileUploadsDirCleaner,
    override val onNeedToRequestPermissions: OnNeedToRequestPermissions,
) : PermissionsFeature {

    private val logger = Logger("FilePicker")

    /**
     * Cache of the current request to be used after permission is granted.
     */
    @VisibleForTesting
    internal var currentRequest: PromptRequest? = null

    /**
     * Handles the file request by building the appropriate intents and starting the file picker.
     *
     * @param promptRequest The [File] prompt request.
     */
    @Suppress("ComplexMethod")
    fun handleFileRequest(promptRequest: File) {
        // Track which permissions are needed.
        val neededPermissions = getNecessaryPermissions(promptRequest)

        // Check if we already have all needed permissions.
        val missingPermissions = neededPermissions.filter {
            !container.context.isPermissionGranted(it)
        }

        if (missingPermissions.isEmpty()) {
            onPermissionsGranted(promptRequest)
        } else {
            askAndroidPermissionsForRequest(neededPermissions, promptRequest)
        }
    }

    /**
     * Returns the necessary permissions for the given [File] prompt request.
     *
     * @param promptRequest The [File] prompt request.
     * @return The set of permissions needed for the prompt request.
     */
    private fun getNecessaryPermissions(promptRequest: File): Set<String> {
        return MimeType.values()
            .filter { it.matches(promptRequest.mimeTypes) }
            .flatMap { mimeType ->
                val permissions = mutableListOf<String>()
                permissions.addAll(mimeType.filePermissions)
                mimeType.capturePermission?.let { permissions.add(it) }
                permissions
            }.toSet()
    }

    /**
     * Handles the file request by building the appropriate intents and starting the file picker.
     *
     * @param intents the list of intents used to build the chooser Intent.
     */
    @VisibleForTesting
    internal fun showChooser(intents: MutableList<Intent>) {
        if (intents.isNotEmpty()) {
            // Combine the intents together using a chooser.
            val lastIntent = intents.removeAt(intents.lastIndex)

            val chooser = Intent.createChooser(lastIntent, null).apply {
                putExtra(EXTRA_INITIAL_INTENTS, intents.toTypedArray())
            }

            container.startActivityForResult(chooser, FILE_PICKER_ACTIVITY_REQUEST_CODE)
        }
    }

    /**
     * Builds a list of intents based on the accepted mime types and capture mode.
     *
     * @param promptRequest The [File] prompt request.
     * @return The list of intents to be used to build the chooser.
     */
    @VisibleForTesting
    internal fun buildIntentList(promptRequest: File): MutableList<Intent> {
        val intents = mutableListOf<Intent>()
        captureUri = null

        // Compare the accepted values against image/*, video/*, and audio/*
        for (type in MimeType.values()) {
            if (type.matches(promptRequest.mimeTypes)) {
                val hasCapturePermission = type.capturePermission?.let {
                    container.context.isPermissionGranted(
                        it,
                    )
                } ?: false

                // The captureMode attribute can be used if the accepted types are exactly for
                // image/*, video/*, or audio/*.
                if (hasCapturePermission && type.shouldCapture(
                        promptRequest.mimeTypes,
                        promptRequest.captureMode,
                    )
                ) {
                    type.buildIntent(container.context, promptRequest)?.also {
                        saveCaptureUriIfPresent(it)
                        container.startActivityForResult(it, FILE_PICKER_ACTIVITY_REQUEST_CODE)
                        return emptyList<Intent>().toMutableList()
                    }
                }

                val hasFilePermissions = container.context.isPermissionGranted(type.filePermissions)

                if (hasFilePermissions) {
                    type.buildIntent(container.context, promptRequest)?.also {
                        saveCaptureUriIfPresent(it)
                        intents.add(it)
                    }
                }

                if (hasCapturePermission) {
                    type.buildCaptureIntent(container.context, promptRequest)?.also {
                        saveCaptureUriIfPresent(it)
                        intents.add(it)
                    }
                }
            }
        }
        return intents
    }

    /**
     * Notifies the feature of intent results for prompt requests handled by
     * other apps like file chooser requests.
     *
     * @param requestCode The code of the app that requested the intent.
     * @param intent The result of the request.
     */
    fun onActivityResult(requestCode: Int, resultCode: Int, intent: Intent?): Boolean {
        var resultHandled = false
        val request = getActivePromptRequest() ?: return false
        if (requestCode == FILE_PICKER_ACTIVITY_REQUEST_CODE && request is File) {
            store.consumePromptFrom(sessionId, request.uid) {
                if (resultCode == RESULT_OK) {
                    handleFilePickerIntentResult(intent, request)
                } else {
                    request.onDismiss()
                }
            }
            resultHandled = true
        }
        if (request !is File) {
            logger.error("Invalid PromptRequest expected File but $request was provided")
        }

        return resultHandled
    }

    private fun getActivePromptRequest(): PromptRequest? =
        store.state.findCustomTabOrSelectedTab(sessionId)?.content?.promptRequests?.lastOrNull { prompt ->
            prompt is File
        }

    /**
     * Notifies the feature that the permissions request was completed. It will then
     * either process or dismiss the prompt request.
     *
     * @param permissions List of permission requested.
     * @param grantResults The grant results for the corresponding permissions
     * @see [onNeedToRequestPermissions].
     */
    override fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        if (grantResults.isNotEmpty() && grantResults.any { it == PERMISSION_GRANTED }) {
            // at least one permission was granted
            onPermissionsGranted(currentRequest as File)
        } else {
            // all permissions were denied, either when requested or already permanently denied
            onPermissionsDenied()
        }
        currentRequest = null
    }

    /**
     * Used in conjunction with [onNeedToRequestPermissions], to notify the feature
     * that all the required permissions have been granted, and the pending [PromptRequest]
     * can be performed.
     *
     * If the required permission has not been granted
     * [onNeedToRequestPermissions] will be called.
     */
    @VisibleForTesting
    internal fun onPermissionsGranted(promptRequest: File) {
        val intents = buildIntentList(promptRequest)
        showChooser(intents)
    }

    /**
     * Used in conjunction with [onNeedToRequestPermissions] to notify the feature that one
     * or more required permissions have been denied.
     */
    @VisibleForTesting
    internal fun onPermissionsDenied() {
        // Nothing left to do. Consume / cleanup the requests.
        store.consumePromptFrom<File>(sessionId) { request ->
            request.onDismiss()
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun handleFilePickerIntentResult(intent: Intent?, request: File) {
        if (intent?.clipData != null && request.isMultipleFilesSelection) {
            intent.clipData?.run {
                val uris = Array<Uri>(itemCount) { index -> getItemAt(index).uri }
                // We want to verify that we are not exposing any private data
                val sanitizedUris = uris.removeUrisUnderPrivateAppDir(container.context)
                if (sanitizedUris.isEmpty()) {
                    request.onDismiss()
                } else {
                    sanitizedUris.map {
                        enqueueForCleanup(container.context, it)
                    }
                    request.onMultipleFilesSelected(container.context, sanitizedUris)
                }
            }
        } else {
            val uri = intent?.data ?: captureUri
            uri?.let {
                // We want to verify that we are not exposing any private data
                if (!it.isUnderPrivateAppDirectory(container.context)) {
                    enqueueForCleanup(container.context, it)
                    request.onSingleFileSelected(container.context, it)
                } else {
                    request.onDismiss()
                }
            } ?: request.onDismiss()
        }

        captureUri = null
    }

    private fun saveCaptureUriIfPresent(intent: Intent) =
        intent.getParcelableExtraCompat(EXTRA_OUTPUT, Uri::class.java)?.let { captureUri = it }

    @VisibleForTesting
    fun askAndroidPermissionsForRequest(permissions: Set<String>, request: File) {
        currentRequest = request
        onNeedToRequestPermissions(permissions.toTypedArray())
    }

    private fun enqueueForCleanup(context: Context, uri: Uri) {
        val contentResolver = context.contentResolver
        val fileName = uri.getFileName(contentResolver)
        fileUploadsDirCleaner.enqueueForCleanup(fileName)
    }

    companion object {
        const val FILE_PICKER_ACTIVITY_REQUEST_CODE = 7113
    }
}

internal fun Array<Uri>.removeUrisUnderPrivateAppDir(context: Context): Array<Uri> {
    return this.filter { !it.isUnderPrivateAppDirectory(context) }.toTypedArray()
}
