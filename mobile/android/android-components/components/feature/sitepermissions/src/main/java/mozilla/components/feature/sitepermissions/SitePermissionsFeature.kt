/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.support.annotation.DrawableRes
import android.support.annotation.StringRes
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
     * Notifies that the list of [grantedPermissions] have been granted for the [session] and [url].
     *
     * @param session the session which requested the permissions.
     * @param url the url which requested the permissions.
     * @param grantedPermissions the list of [grantedPermissions] that have been granted.
     */
    private fun onContentPermissionGranted(session: Session, url: String, grantedPermissions: List<Permission>) {
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
     * @param url the url which requested the permissions.
     */
    private fun onContentPermissionDeny(session: Session, url: String) {
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
        return if (!permissionRequest.containsVideoAndAudioSources()) {
            val prompt = handlingSingleContentPermissions(session, permissionRequest)
            prompt
        } else {
            TODO()
            // Related issues:
            // https://github.com/mozilla-mobile/android-components/issues/1952
            // https://github.com/mozilla-mobile/android-components/issues/1954
            // https://github.com/mozilla-mobile/android-components/issues/1951
        }
    }

    private fun handlingSingleContentPermissions(
        session: Session,
        permissionRequest: PermissionRequest
    ): DoorhangerPrompt {
        val context = anchorView.context
        val permission = permissionRequest.permissions.first()
        val url = permissionRequest.uri ?: ""
        val uri = Uri.parse(permissionRequest.uri ?: url)
        val uriString = " ${uri.host}"

        val allowButtonTitle = context.getString(R.string.mozac_feature_sitepermissions_allow)
        val denyString = context.getString(R.string.mozac_feature_sitepermissions_not_allow)

        val allowButton = Button(allowButtonTitle, true) {
            onContentPermissionGranted(session, url, permissionRequest.permissions)
        }

        val denyButton = Button(denyString) {
            onContentPermissionDeny(session, url)
        }

        val buttons = listOf(denyButton, allowButton)

        val prompt = when (permission) {
            is ContentGeoLocation -> {
                createSinglePermissionPrompt(context,
                        uriString,
                        R.string.mozac_feature_sitepermissions_location_title,
                        R.drawable.mozac_ic_location,
                        buttons)
            }
            is ContentNotification -> {
                createSinglePermissionPrompt(
                        context,
                        uriString,
                        R.string.mozac_feature_sitepermissions_notification_title,
                        R.drawable.mozac_ic_notification,
                        buttons
                )
            }
            is ContentAudioCapture -> {
                createPromptForMicrophonePermission(
                    context,
                    uriString,
                    buttons
                )
            }
            is ContentAudioMicrophone -> {
                createPromptForMicrophonePermission(
                    context,
                    uriString,
                    buttons
                )
            }
            is ContentVideoCapture -> {
                createPromptForCameraPermission(
                    context,
                    uriString,
                    permissionRequest,
                    session,
                    allowButtonTitle,
                    denyButton
                )
            }
            is ContentVideoCamera -> {
                createPromptForCameraPermission(
                    context,
                    uriString,
                    permissionRequest,
                    session,
                    allowButtonTitle,
                    denyButton
                )
            }
            else ->
                throw InvalidParameterException("$permission is not a valid permission.")
        }

        prompt.createDoorhanger(context).show(anchorView)
        return prompt
    }

    @SuppressLint("VisibleForTests")
    private fun createSinglePermissionPrompt(
        context: Context,
        uriString: String,
        @StringRes titleId: Int,
        @DrawableRes iconId: Int,
        buttons: List<DoorhangerPrompt.Button>
    ): DoorhangerPrompt {
        val title = context.getString(titleId, uriString)
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
        uriString: String,
        buttons: List<Button>
    ): DoorhangerPrompt {

        val title = context.getString(R.string.mozac_feature_sitepermissions_microfone_title, uriString)
        val optionTitle = context.getString(R.string.mozac_feature_sitepermissions_option_microphone_one)
        val drawable = ContextCompat.getDrawable(context, R.drawable.mozac_ic_microphone)

        val microphoneRadioButton = RadioButton(optionTitle)
        val controlGroup = ControlGroup(controls = listOf(microphoneRadioButton))

        return DoorhangerPrompt(
            title = title,
            icon = drawable,
            controlGroups = listOf(controlGroup),
            buttons = buttons
        )
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

        val (controlGroup, allowButton) = createControlGroupAndAllowButtonForCameraPermission(
            permissionRequest.cameraPermissions,
            allowButtonTitle,
            session
        )

        return DoorhangerPrompt(
            title = title,
            icon = drawable,
            controlGroups = listOf(controlGroup),
            buttons = listOf(denyButton, allowButton)
        )
    }

    private fun createControlGroupAndAllowButtonForCameraPermission(
        cameraPermissions: List<Permission>,
        allowButtonTitle: String,
        session: Session
    ): Pair<ControlGroup, Button> {

        val (titleOption1, titleOption2) = getCameraTextOptions(cameraPermissions)

        val option1 = RadioButton(titleOption1)

        val option2 = RadioButton(titleOption2)

        val allowButton = Button(allowButtonTitle, true) {
            val selectedPermission: Permission = if (option1.checked) {
                cameraPermissions[0]
            } else {
                cameraPermissions[1]
            }

            onContentPermissionGranted(session, session.url, listOf(selectedPermission))
        }

        return ControlGroup(controls = listOf(option1, option2)) to allowButton
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
