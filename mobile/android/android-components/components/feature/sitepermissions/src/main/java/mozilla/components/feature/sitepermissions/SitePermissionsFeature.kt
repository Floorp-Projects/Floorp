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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentAudioMicrophone
import mozilla.components.concept.engine.permission.Permission.ContentGeoLocation
import mozilla.components.concept.engine.permission.Permission.ContentNotification
import mozilla.components.concept.engine.permission.Permission.ContentVideoCamera
import mozilla.components.concept.engine.permission.Permission.ContentVideoCapture
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.feature.sitepermissions.SitePermissions.Status.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.BLOCKED
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlin.toUri
import mozilla.components.ui.doorhanger.DoorhangerPrompt
import mozilla.components.ui.doorhanger.DoorhangerPrompt.Button
import mozilla.components.ui.doorhanger.DoorhangerPrompt.Control
import mozilla.components.ui.doorhanger.DoorhangerPrompt.Control.CheckBox
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
 * @property storage the object in charge of persisting all the [SitePermissions] objects.
 * @property sitePermissionsRules indicates how permissions should behave per permission category.
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested. Once the request is completed, [onPermissionsResult] needs to be invoked.
 **/

@Suppress("TooManyFunctions", "LargeClass")
class SitePermissionsFeature(
    private val anchorView: View,
    private val sessionManager: SessionManager,
    private val storage: SitePermissionsStorage = SitePermissionsStorage(anchorView.context),
    var sitePermissionsRules: SitePermissionsRules? = null,
    private val onNeedToRequestPermissions: OnNeedToRequestPermissions
) : LifecycleAwareFeature {

    private val observer = SitePermissionsRequestObserver(sessionManager, feature = this)
    internal val ioCoroutineScope by lazy { CoroutineScope(Dispatchers.IO) }

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
     * @param shouldStore indicates weather the permission should be stored.
     * If it's true none prompt will show otherwise the prompt will be shown.
     */
    private fun onContentPermissionGranted(
        session: Session,
        grantedPermissions: List<Permission>,
        shouldStore: Boolean
    ) {
        session.contentPermissionRequest.consume { request ->
            request.grant()

            if (shouldStore) {
                ioCoroutineScope.launch {
                    storeSitePermissions(request, grantedPermissions, ALLOWED)
                }
            }
            true
        }
    }

    @Synchronized internal fun storeSitePermissions(
        request: PermissionRequest,
        permissions: List<Permission> = request.permissions,
        status: SitePermissions.Status
    ) {
        var sitePermissions = storage.findSitePermissionsBy(request.host)

        if (sitePermissions == null) {
            sitePermissions = request.toSitePermissions(
                status = status,
                permissions = permissions
            )
            storage.save(sitePermissions)
        } else {
            sitePermissions = request.toSitePermissions(status, sitePermissions)
            storage.update(sitePermissions)
        }
    }

    /**
     * Notifies that the permissions requested by this [session] were rejected.
     *
     * @param session the session which requested the permissions.
     * @param shouldStore indicates weather the permission should be stored.
     */
    private fun onContentPermissionDeny(session: Session, shouldStore: Boolean) {
        session.contentPermissionRequest.consume { request ->
            request.reject()

            if (shouldStore) {
                ioCoroutineScope.launch {
                    storeSitePermissions(request = request, status = BLOCKED)
                }
            }
            true
        }
    }

    internal suspend fun onContentPermissionRequested(
        session: Session,
        request: PermissionRequest
    ): DoorhangerPrompt? {

        return if (shouldApplyRules(request.host)) {
            handleRuledFlow(request, session)
        } else {
            handleNoRuledFlow(request, session)
        }
    }

    @VisibleForTesting
    internal fun findDoNotAskAgainCheckBox(controls: List<Control>?): CheckBox? {
        return controls?.find {
            (it is CheckBox)
        } as CheckBox?
    }

    private suspend fun handleNoRuledFlow(
        permissionRequest: PermissionRequest,
        session: Session
    ): DoorhangerPrompt? {
        val permissionFromStorage = withContext(ioCoroutineScope.coroutineContext) {
            storage.findSitePermissionsBy(permissionRequest.host)
        }

        return if (shouldShowPrompt(permissionRequest, permissionFromStorage)) {
            createPrompt(permissionRequest, session)
        } else {
            if (permissionFromStorage.isGranted(permissionRequest)) {
                permissionRequest.grant()
            } else {
                permissionRequest.reject()
            }
            session.contentPermissionRequest.consume { true }
            null
        }
    }

    private fun shouldShowPrompt(
        permissionRequest: PermissionRequest,
        permissionFromStorage: SitePermissions?
    ): Boolean {
        return (permissionRequest.containsVideoAndAudioSources() ||
            permissionFromStorage == null ||
            !permissionRequest.doNotAskAgain(permissionFromStorage))
    }

    private fun handleRuledFlow(permissionRequest: PermissionRequest, session: Session): DoorhangerPrompt? {
        val action = requireNotNull(sitePermissionsRules).getActionFrom(permissionRequest)
        return when (action) {
            SitePermissionsRules.Action.BLOCKED -> {
                permissionRequest.reject()
                session.contentPermissionRequest.consume { true }
                null
            }
            SitePermissionsRules.Action.ASK_TO_ALLOW -> {
                createPrompt(permissionRequest, session)
            }
        }
    }

    private fun shouldApplyRules(host: String) =
        sitePermissionsRules != null && !requireNotNull(sitePermissionsRules).isHostInExceptions(host)

    private fun PermissionRequest.doNotAskAgain(permissionFromStore: SitePermissions): Boolean {
        return permissions.any { permission ->
            when (permission) {
                is ContentGeoLocation -> {
                    permissionFromStore.location.doNotAskAgain()
                }
                is ContentAudioCapture, is ContentAudioMicrophone -> {
                    permissionFromStore.microphone.doNotAskAgain()
                }
                is ContentVideoCamera, is ContentVideoCapture -> {
                    permissionFromStore.cameraFront.doNotAskAgain() &&
                        permissionFromStore.cameraBack.doNotAskAgain()
                }
                else -> false
            }
        }
    }

    private fun PermissionRequest.toSitePermissions(
        status: SitePermissions.Status,
        initialSitePermission: SitePermissions = SitePermissions(
            host,
            savedAt = System.currentTimeMillis()
        ),
        permissions: List<Permission> = this.permissions
    ): SitePermissions {
        var sitePermissions = initialSitePermission
        for (permission in permissions) {
            sitePermissions = updateSitePermissionsStatus(status, permission, sitePermissions)
        }
        return sitePermissions
    }

    private fun updateSitePermissionsStatus(
        status: SitePermissions.Status,
        permission: Permission,
        sitePermissions: SitePermissions
    ): SitePermissions {
        return when (permission) {
            is ContentGeoLocation -> {
                sitePermissions.copy(location = status)
            }
            is ContentNotification -> {
                sitePermissions.copy(notification = status)
            }
            is ContentAudioCapture, is ContentAudioMicrophone -> {
                sitePermissions.copy(microphone = status)
            }
            is ContentVideoCamera, is ContentVideoCapture -> {
                if (permission.isFrontCamera) {
                    sitePermissions.copy(cameraFront = status)
                } else {
                    sitePermissions.copy(cameraBack = status)
                }
            }
            else ->
                throw InvalidParameterException("$permission is not a valid permission.")
        }
    }

    private fun createPrompt(
        permissionRequest: PermissionRequest,
        session: Session
    ): DoorhangerPrompt {
        val host = permissionRequest.host
        val context = anchorView.context
        val allowButtonTitle = context.getString(R.string.mozac_feature_sitepermissions_allow)
        val denyString = context.getString(R.string.mozac_feature_sitepermissions_not_allow)

        var prompt: DoorhangerPrompt? = null

        val allowButton = Button(allowButtonTitle, true) {
            val shouldStore = shouldStorePermission(permissionRequest, prompt)
            onContentPermissionGranted(session, permissionRequest.permissions, shouldStore)
        }

        val denyButton = Button(denyString) {
            val shouldStore = shouldStorePermission(permissionRequest, prompt)
            onContentPermissionDeny(session, shouldStore)
        }

        prompt = if (!permissionRequest.containsVideoAndAudioSources()) {
            handlingSingleContentPermissions(session, permissionRequest, host, allowButton, denyButton)
        } else {
            createVideoAndAudioPrompt(session, permissionRequest, host, allowButtonTitle, denyButton)
        }

        prompt.createDoorhanger(context).show(anchorView)

        return prompt
    }

    private fun shouldStorePermission(
        permissionRequest: PermissionRequest,
        prompt: DoorhangerPrompt?
    ): Boolean {
        return if (permissionRequest.shouldIncludeDoNotAskAgainCheckBox()) {
            getDoNotAskAgainCheckBoxValue(permissionRequest, prompt)
        } else {
            true
        }
    }

    private fun getDoNotAskAgainCheckBoxValue(
        permissionRequest: PermissionRequest,
        prompt: DoorhangerPrompt?
    ): Boolean {
        return if (permissionRequest.shouldIncludeDoNotAskAgainCheckBox()) {
            val controls = prompt?.controlGroups?.first()?.controls
            findDoNotAskAgainCheckBox(controls)?.checked ?: false
        } else {
            false
        }
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
        val microphoneControlGroups = createControlGroupForMicrophonePermission(
            icon = microphoneIcon,
            shouldIncludeDoNotAskAgainCheckBox = false
        )

        val cameraIcon = ContextCompat.getDrawable(context, R.drawable.mozac_ic_video)
        val cameraControlGroup = createControlGroupForCameraPermission(
            icon = cameraIcon,
            cameraPermissions = permissionRequest.cameraPermissions,
            shouldIncludeDoNotAskAgainCheckBox = false
        )

        val allowButton = Button(allowButtonTitle, true) {
            val selectedCameraPermission: Permission =
                findSelectedPermission(cameraControlGroup, permissionRequest.cameraPermissions)

            val selectedMicrophonePermission: Permission =
                findSelectedPermission(microphoneControlGroups, permissionRequest.microphonePermissions)

            onContentPermissionGranted(session, listOf(selectedCameraPermission, selectedMicrophonePermission), false)
        }

        return DoorhangerPrompt(
            title = title,
            controlGroups = listOf(cameraControlGroup, microphoneControlGroups),
            buttons = listOf(denyButton, allowButton)
        )
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
                createSinglePermissionPrompt(
                    context,
                    host,
                    R.string.mozac_feature_sitepermissions_location_title,
                    R.drawable.mozac_ic_location,
                    buttons,
                    shouldIncludeDoNotAskAgainCheckBox = true
                )
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

    @Suppress("LongParameterList")
    @SuppressLint("VisibleForTests")
    private fun createSinglePermissionPrompt(
        context: Context,
        host: String,
        @StringRes titleId: Int,
        @DrawableRes iconId: Int,
        buttons: List<DoorhangerPrompt.Button>,
        shouldIncludeDoNotAskAgainCheckBox: Boolean = false
    ): DoorhangerPrompt {
        val title = context.getString(titleId, host)
        val drawable = ContextCompat.getDrawable(context, iconId)

        val controlGroups = mutableListOf<ControlGroup>()

        if (shouldIncludeDoNotAskAgainCheckBox) {
            val checkBox = createDoNotAskAgainCheckBox(anchorView.context)
            controlGroups += ControlGroup(controls = listOf(checkBox))
        }

        return DoorhangerPrompt(
            title = title,
            icon = drawable,
            buttons = buttons,
            controlGroups = controlGroups
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
        val controlGroup = createControlGroupForMicrophonePermission(shouldIncludeDoNotAskAgainCheckBox = true)

        return DoorhangerPrompt(
            title = title,
            icon = drawable,
            controlGroups = listOf(controlGroup),
            buttons = buttons
        )
    }

    private fun createControlGroupForMicrophonePermission(
        icon: Drawable? = null,
        shouldIncludeDoNotAskAgainCheckBox: Boolean
    ): ControlGroup {
        val context = anchorView.context
        val optionTitle = context.getString(R.string.mozac_feature_sitepermissions_option_microphone_one)
        val microphoneRadioButton = RadioButton(optionTitle)

        val controls = mutableListOf<Control>(microphoneRadioButton)
        if (shouldIncludeDoNotAskAgainCheckBox) {
            controls.add(createDoNotAskAgainCheckBox(anchorView.context))
        }

        return ControlGroup(icon, controls = controls)
    }

    private fun createDoNotAskAgainCheckBox(context: Context): CheckBox {
        val doNotAskAgainTitle = context.getString(
            R.string.mozac_feature_sitepermissions_do_not_ask_again_on_this_site2)
        return CheckBox(doNotAskAgainTitle, true)
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
            cameraPermissions = permissionRequest.cameraPermissions,
            shouldIncludeDoNotAskAgainCheckBox = true
        )

        val allowButton = Button(allowButtonTitle, true) {
            val selectedPermission: Permission =
                findSelectedPermission(controlGroup, permissionRequest.cameraPermissions)
            val doNotAskAgain = findDoNotAskAgainCheckBox(controlGroup.controls)?.checked ?: false
            onContentPermissionGranted(session, listOf(selectedPermission), doNotAskAgain)
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
            if (control is RadioButton && control.checked) {
                return permissions[index]
            }
        }
        throw NoSuchElementException("Unable to find the selected permission on $permissions")
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun createControlGroupForCameraPermission(
        icon: Drawable? = null,
        cameraPermissions: List<Permission>,
        shouldIncludeDoNotAskAgainCheckBox: Boolean
    ): ControlGroup {

        val (titleOption1, titleOption2) = getCameraTextOptions(cameraPermissions)

        val option1 = RadioButton(titleOption1)

        val option2 = RadioButton(titleOption2)

        val controls = mutableListOf<Control>(option1, option2)
        if (shouldIncludeDoNotAskAgainCheckBox) {
            controls.add(createDoNotAskAgainCheckBox(anchorView.context))
        }
        return ControlGroup(icon, controls = controls)
    }

    private fun getCameraTextOptions(cameraPermissions: List<Permission>): Pair<String, String> {
        val context = anchorView.context
        val option1Text = if (cameraPermissions[0].isBackCamera) {
            R.string.mozac_feature_sitepermissions_back_facing_camera2
        } else {
            R.string.mozac_feature_sitepermissions_selfie_camera2
        }

        val option2Text = if (cameraPermissions[1].isBackCamera) {
            R.string.mozac_feature_sitepermissions_back_facing_camera2
        } else {
            R.string.mozac_feature_sitepermissions_selfie_camera2
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

    private val PermissionRequest.host get() = uri?.toUri()?.host ?: ""
    private val Permission.isBackCamera get() = desc?.contains("back") ?: false
    private val Permission.isFrontCamera get() = desc?.contains("front") ?: false

    internal class SitePermissionsRequestObserver(
        sessionManager: SessionManager,
        private val feature: SitePermissionsFeature
    ) : SelectionAwareSessionObserver(sessionManager) {

        override fun onContentPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
            runBlocking {
                feature.onContentPermissionRequested(session, permissionRequest)
            }
            return false
        }

        override fun onAppPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
            return feature.onAppPermissionRequested(permissionRequest)
        }
    }
}

internal fun SitePermissions?.isGranted(permissionRequest: PermissionRequest): Boolean {
    return if (this != null) {
        permissionRequest.permissions.all { permission ->
            isPermissionGranted(permission, this)
        }
    } else {
        false
    }
}

private fun isPermissionGranted(
    permission: Permission,
    permissionFromStorage: SitePermissions
): Boolean {
    return when (permission) {
        is ContentGeoLocation -> {
            permissionFromStorage.location.isAllowed()
        }
        is ContentNotification -> {
            permissionFromStorage.notification.isAllowed()
        }
        is ContentAudioCapture, is ContentAudioMicrophone -> {
            permissionFromStorage.microphone.isAllowed()
        }
        is ContentVideoCamera, is ContentVideoCapture -> {
            permissionFromStorage.cameraBack.isAllowed() || permissionFromStorage.cameraFront.isAllowed()
        }
        else ->
            throw InvalidParameterException("$permission is not a valid permission.")
    }
}

@VisibleForTesting
internal fun PermissionRequest.shouldIncludeDoNotAskAgainCheckBox(): Boolean {
    return if (this.containsVideoAndAudioSources()) {
        false
    } else {
        this.permissions.any { permission ->
            when (permission) {
                is ContentGeoLocation,
                is ContentVideoCamera,
                is ContentVideoCapture,
                is ContentAudioCapture,
                is ContentAudioMicrophone -> {
                    true
                }
                else -> false
            }
        }
    }
}
