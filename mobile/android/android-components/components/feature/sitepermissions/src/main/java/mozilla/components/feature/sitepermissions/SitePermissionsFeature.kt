/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.drawable.Drawable
import android.support.annotation.DrawableRes
import android.support.annotation.StringRes
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import android.support.v4.content.ContextCompat
import android.view.View
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.ContentGeoLocation
import mozilla.components.concept.engine.permission.Permission.ContentNotification
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentAudioMicrophone
import mozilla.components.concept.engine.permission.Permission.ContentVideoCamera
import mozilla.components.concept.engine.permission.Permission.ContentVideoCapture
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlin.toUri
import mozilla.components.ui.doorhanger.DoorhangerPrompt
import mozilla.components.ui.doorhanger.DoorhangerPrompt.Button
import mozilla.components.ui.doorhanger.DoorhangerPrompt.Control.RadioButton
import mozilla.components.ui.doorhanger.DoorhangerPrompt.ControlGroup
import java.security.InvalidParameterException

typealias OnNeedToRequestPermissions = (permissions: Array<String>) -> Unit

/**
 * This feature will subscribe to the currently selected [Session] and display
 * a suitable dialogs based on [Session.Observer.onAppPermissionRequested] or
 * [Session.Observer.onContentPermissionRequested]  events.
 * Once the dialog is closed the [PermissionRequest] will be consumed.
 *
 * @property anchorView the view which the prompt is going to be anchored.
 * @property sessionManager the [SessionManager] instance in order to subscribe
 * to the selected [Session].
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested. Once the request is completed, [onPermissionsResult] needs to be invoked.
 **/

@Suppress("TooManyFunctions")
class SitePermissionsFeature(
    private val anchorView: View,
    private val sessionManager: SessionManager,
    private val onNeedToRequestPermissions: OnNeedToRequestPermissions
) : LifecycleAwareFeature {

    private val observer = SitePermissionsRequestObserver(sessionManager, feature = this)

    override fun start() {
        observer.observeSelected()
    }

    override fun stop() {
        observer.stop()
    }

    /**
     * Notifies the feature that the permissions requested were completed.
     *
     * @param grantResults the grant results for the corresponding permissions
     * @see [onNeedToRequestPermissions].
     */
    fun onPermissionsResult(grantResults: IntArray) {
        sessionManager.selectedSession?.apply {
            appPermissionRequest.consume { permissionsRequest ->

                val allPermissionWereGranted = grantResults.all { grantResult ->
                    grantResult == PackageManager.PERMISSION_GRANTED
                }

                if (grantResults.isNotEmpty() && allPermissionWereGranted) {
                    permissionsRequest.grant()
                } else {
                    permissionsRequest.reject()
                }
                true
            }
        }
    }

    /**
     * Notifies that the list of [grantedPermissions] have been granted for the [session].
     *
     * @param session the session which requested the permissions.
     * @param grantedPermissions the list of [grantedPermissions] that have been granted.
     */
    private fun onContentPermissionGranted(session: Session, grantedPermissions: List<Permission>) {
        session.contentPermissionRequest.consume {
            it.grant()
            // Update the DB
            true
        }
    }

    /**
     * Notifies that the permissions requested by this [session] were rejected.
     *
     * @param session the session which requested the permissions.
     */
    private fun onContentPermissionDeny(session: Session) {
        session.contentPermissionRequest.consume {
            it.reject()
            // Update the DB
            true
        }
    }

    internal fun onContentPermissionRequested(
        session: Session,
        permissionRequest: PermissionRequest
    ): DoorhangerPrompt {
        val context = anchorView.context
        val host = permissionRequest.uri?.toUri()?.host ?: ""
        val allowButtonTitle = context.getString(R.string.mozac_feature_sitepermissions_allow)
        val denyString = context.getString(R.string.mozac_feature_sitepermissions_not_allow)

        val allowButton = Button(allowButtonTitle, true) {
            onContentPermissionGranted(session, permissionRequest.permissions)
        }

        val denyButton = Button(denyString) {
            onContentPermissionDeny(session)
        }

        val prompt = if (!permissionRequest.containsVideoAndAudioSources()) {
            handlingSingleContentPermissions(session, permissionRequest, host, allowButton, denyButton)
        } else {
            createVideoAndAudioPrompt(session, permissionRequest, host, allowButtonTitle, denyButton)
        }

        prompt.createDoorhanger(context).show(anchorView)
        return prompt
    }

    @SuppressLint("VisibleForTests")
    private fun createVideoAndAudioPrompt(
        session: Session,
        permissionRequest: PermissionRequest,
        host: String,
        allowButtonTitle: String,
        denyButton: Button
    ): DoorhangerPrompt {
        val context = anchorView.context
        val title = context.getString(R.string.mozac_feature_sitepermissions_microfone_title, host)

        val microphoneIcon = ContextCompat.getDrawable(context, R.drawable.mozac_ic_microphone)
        val microphoneControlGroups = createControlGroupForMicrophonePermission(microphoneIcon)

        val cameraIcon = ContextCompat.getDrawable(context, R.drawable.mozac_ic_video)
        val cameraControlGroup = createControlGroupForCameraPermission(
            icon = cameraIcon,
            cameraPermissions = permissionRequest.cameraPermissions
        )

        val allowButton = Button(allowButtonTitle, true) {
            val selectedCameraPermission: Permission =
                findSelectedPermission(cameraControlGroup, permissionRequest.cameraPermissions)

            val selectedMicrophonePermission: Permission =
                findSelectedPermission(microphoneControlGroups, permissionRequest.microphonePermissions)

            onContentPermissionGranted(session, listOf(selectedCameraPermission, selectedMicrophonePermission))
        }

        val prompt = DoorhangerPrompt(
            title = title,
            controlGroups = listOf(cameraControlGroup, microphoneControlGroups),
            buttons = listOf(denyButton, allowButton)
        )
        return prompt
    }

    private fun handlingSingleContentPermissions(
        session: Session,
        permissionRequest: PermissionRequest,
        host: String,
        allowButton: Button,
        denyButton: Button
    ): DoorhangerPrompt {
        val context = anchorView.context
        val permission = permissionRequest.permissions.first()
        val buttons = listOf(denyButton, allowButton)

        return when (permission) {
            is ContentGeoLocation -> {
                createSinglePermissionPrompt(context,
                    host,
                    R.string.mozac_feature_sitepermissions_location_title,
                    R.drawable.mozac_ic_location,
                    buttons)
            }
            is ContentNotification -> {
                createSinglePermissionPrompt(
                    context,
                    host,
                    R.string.mozac_feature_sitepermissions_notification_title,
                    R.drawable.mozac_ic_notification,
                    buttons
                )
            }
            is ContentAudioCapture, is ContentAudioMicrophone -> {
                createPromptForMicrophonePermission(
                    context,
                    host,
                    buttons
                )
            }
            is ContentVideoCamera, is ContentVideoCapture -> {
                createPromptForCameraPermission(
                    context,
                    host,
                    permissionRequest,
                    session,
                    allowButton.label,
                    denyButton
                )
            }
            else ->
                throw InvalidParameterException("$permission is not a valid permission.")
        }
    }

    @SuppressLint("VisibleForTests")
    private fun createSinglePermissionPrompt(
        context: Context,
        host: String,
        @StringRes titleId: Int,
        @DrawableRes iconId: Int,
        buttons: List<DoorhangerPrompt.Button>
    ): DoorhangerPrompt {
        val title = context.getString(titleId, host)
        val drawable = ContextCompat.getDrawable(context, iconId)

        return DoorhangerPrompt(
            title = title,
            icon = drawable,
            buttons = buttons
        )
    }

    @SuppressLint("VisibleForTests")
    private fun createPromptForMicrophonePermission(
        context: Context,
        host: String,
        buttons: List<Button>
    ): DoorhangerPrompt {

        val title = context.getString(R.string.mozac_feature_sitepermissions_microfone_title, host)
        val drawable = ContextCompat.getDrawable(context, R.drawable.mozac_ic_microphone)
        val controlGroup = createControlGroupForMicrophonePermission()

        return DoorhangerPrompt(
            title = title,
            icon = drawable,
            controlGroups = listOf(controlGroup),
            buttons = buttons
        )
    }

    private fun createControlGroupForMicrophonePermission(icon: Drawable? = null): ControlGroup {
        val context = anchorView.context
        val optionTitle = context.getString(R.string.mozac_feature_sitepermissions_option_microphone_one)
        val microphoneRadioButton = RadioButton(optionTitle)
        return ControlGroup(icon, controls = listOf(microphoneRadioButton))
    }

    @Suppress("LongParameterList")
    @SuppressLint("VisibleForTests")
    private fun createPromptForCameraPermission(
        context: Context,
        uriString: String,
        permissionRequest: PermissionRequest,
        session: Session,
        allowButtonTitle: String,
        denyButton: Button
    ): DoorhangerPrompt {

        val title = context.getString(R.string.mozac_feature_sitepermissions_microfone_title, uriString)
        val drawable = ContextCompat.getDrawable(context, R.drawable.mozac_ic_video)

        val controlGroup = createControlGroupForCameraPermission(
            cameraPermissions = permissionRequest.cameraPermissions
        )

        val allowButton = Button(allowButtonTitle, true) {
            val selectedPermission: Permission =
                findSelectedPermission(controlGroup, permissionRequest.cameraPermissions)

            onContentPermissionGranted(session, listOf(selectedPermission))
        }

        return DoorhangerPrompt(
            title = title,
            icon = drawable,
            controlGroups = listOf(controlGroup),
            buttons = listOf(denyButton, allowButton)
        )
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun findSelectedPermission(controlGroup: ControlGroup, permissions: List<Permission>): Permission {
        controlGroup.controls.forEachIndexed { index, control ->
            val radioButton = control as RadioButton
            if (radioButton.checked) {
                return permissions[index]
            }
        }
        throw NoSuchElementException("Unable to find the selected permission on $permissions")
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun createControlGroupForCameraPermission(
        icon: Drawable? = null,
        cameraPermissions: List<Permission>
    ): ControlGroup {

        val (titleOption1, titleOption2) = getCameraTextOptions(cameraPermissions)

        val option1 = RadioButton(titleOption1)

        val option2 = RadioButton(titleOption2)

        return ControlGroup(icon, controls = listOf(option1, option2))
    }

    private fun getCameraTextOptions(cameraPermissions: List<Permission>): Pair<String, String> {
        val context = anchorView.context
        val option1Text = if (cameraPermissions[0].desc?.contains("back") == true) {
            R.string.mozac_feature_sitepermissions_back_facing_camera
        } else {
            R.string.mozac_feature_sitepermissions_selfie_camera
        }

        val option2Text = if (cameraPermissions[1].desc?.contains("back") == true) {
            R.string.mozac_feature_sitepermissions_back_facing_camera
        } else {
            R.string.mozac_feature_sitepermissions_selfie_camera
        }
        return context.getString(option1Text) to context.getString(option2Text)
    }

    private val PermissionRequest.cameraPermissions: List<Permission>
        get() {
            return permissions.filter {
                it is ContentVideoCamera || it is ContentVideoCapture
            }
        }

    private val PermissionRequest.microphonePermissions: List<Permission>
        get() {
            return permissions.filter {
                it is ContentAudioCapture || it is ContentAudioMicrophone
            }
        }

    private fun onAppPermissionRequested(permissionRequest: PermissionRequest): Boolean {
        val permissions = permissionRequest.permissions.map { it.id ?: "" }
        onNeedToRequestPermissions(permissions.toTypedArray())
        return false
    }

    internal class SitePermissionsRequestObserver(
        sessionManager: SessionManager,
        private val feature: SitePermissionsFeature
    ) : SelectionAwareSessionObserver(sessionManager) {

        override fun onContentPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
            feature.onContentPermissionRequested(session, permissionRequest)
            return false
        }

        override fun onAppPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
            return feature.onAppPermissionRequested(permissionRequest)
        }
    }
}
