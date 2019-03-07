/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.Manifest.permission.RECORD_AUDIO
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.view.View
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
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
import mozilla.components.support.ktx.android.content.isPermissionGranted
import mozilla.components.support.ktx.kotlin.toUri
import mozilla.components.ui.doorhanger.DoorhangerPrompt
import mozilla.components.ui.doorhanger.DoorhangerPrompt.Button
import mozilla.components.ui.doorhanger.DoorhangerPrompt.Control
import mozilla.components.ui.doorhanger.DoorhangerPrompt.Control.CheckBox
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
    internal val ioCoroutineScope by lazy { coroutineScopeInitializer() }

    internal var coroutineScopeInitializer = {
        CoroutineScope(Dispatchers.IO)
    }

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
                storeSitePermissions(session, request, grantedPermissions, ALLOWED)
            }
            true
        }
    }

    @Synchronized internal fun storeSitePermissions(
        session: Session,
        request: PermissionRequest,
        permissions: List<Permission> = request.permissions,
        status: SitePermissions.Status
    ) {
        if (session.private) {
            return
        }

        ioCoroutineScope.launch {
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
                storeSitePermissions(session, request = request, status = BLOCKED)
            }
            true
        }
    }

    internal suspend fun onContentPermissionRequested(
        session: Session,
        request: PermissionRequest
    ): DoorhangerPrompt? {

        // Preventing this behavior https://github.com/mozilla-mobile/android-components/issues/2668
        if (request.isMicrophone && isMicrophoneAndroidPermissionNotGranted) {
            request.reject()
            return null
        }

        val permissionFromStorage = withContext(ioCoroutineScope.coroutineContext) {
            storage.findSitePermissionsBy(request.host)
        }

        return if (shouldApplyRules(permissionFromStorage)) {
            handleRuledFlow(request, session)
        } else {
            handleNoRuledFlow(permissionFromStorage, request, session)
        }
    }

    @VisibleForTesting
    internal fun findDoNotAskAgainCheckBox(controls: List<Control>?): CheckBox? {
        return controls?.find {
            (it is CheckBox)
        } as CheckBox?
    }

    private fun handleNoRuledFlow(
        permissionFromStorage: SitePermissions?,
        permissionRequest: PermissionRequest,
        session: Session
    ): DoorhangerPrompt? {
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
        return (permissionFromStorage == null || !permissionRequest.doNotAskAgain(permissionFromStorage))
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

    private fun shouldApplyRules(permissionFromStorage: SitePermissions?) =
        sitePermissionsRules != null && permissionFromStorage == null

    private fun PermissionRequest.doNotAskAgain(permissionFromStore: SitePermissions): Boolean {
        return permissions.any { permission ->
            when (permission) {
                is ContentGeoLocation -> {
                    permissionFromStore.location.doNotAskAgain()
                }
                is ContentNotification -> {
                    permissionFromStore.notification.doNotAskAgain()
                }
                is ContentAudioCapture, is ContentAudioMicrophone -> {
                    permissionFromStore.microphone.doNotAskAgain()
                }
                is ContentVideoCamera, is ContentVideoCapture -> {
                    permissionFromStore.camera.doNotAskAgain()
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
                sitePermissions.copy(camera = status)
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
            val permission = permissionRequest.permissions.first()
            handlingSingleContentPermissions(permission, host, allowButton, denyButton)
        } else {
            createVideoAndAudioPrompt(host, allowButton, denyButton)
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

    private fun createVideoAndAudioPrompt(
        host: String,
        allowButton: Button,
        denyButton: Button
    ): DoorhangerPrompt {
        val context = anchorView.context
        return createSinglePermissionPrompt(
            context,
            host,
            R.string.mozac_feature_sitepermissions_camera_and_microphone,
            R.drawable.mozac_ic_microphone,
            listOf(denyButton, allowButton),
            shouldIncludeDoNotAskAgainCheckBox = true
        )
    }

    private fun handlingSingleContentPermissions(
        permission: Permission,
        host: String,
        allowButton: Button,
        denyButton: Button
    ): DoorhangerPrompt {
        val context = anchorView.context
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
                createSinglePermissionPrompt(
                    context,
                    host,
                    R.string.mozac_feature_sitepermissions_microfone_title,
                    R.drawable.mozac_ic_microphone,
                    buttons,
                    shouldIncludeDoNotAskAgainCheckBox = true
                )
            }
            is ContentVideoCamera, is ContentVideoCapture -> {
                createSinglePermissionPrompt(
                    context,
                    host,
                    R.string.mozac_feature_sitepermissions_camera_title,
                    R.drawable.mozac_ic_video,
                    buttons,
                    shouldIncludeDoNotAskAgainCheckBox = true
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

    private fun createDoNotAskAgainCheckBox(context: Context): CheckBox {
        val doNotAskAgainTitle = context.getString(
            R.string.mozac_feature_sitepermissions_do_not_ask_again_on_this_site2
        )
        return CheckBox(doNotAskAgainTitle, true)
    }

    private fun onAppPermissionRequested(permissionRequest: PermissionRequest): Boolean {
        val permissions = permissionRequest.permissions.map { it.id ?: "" }
        onNeedToRequestPermissions(permissions.toTypedArray())
        return false
    }

    private val PermissionRequest.host get() = uri?.toUri()?.host ?: ""
    private val PermissionRequest.isMicrophone: Boolean
        get() {
            if (containsVideoAndAudioSources()) {
                return false
            }
            return when (permissions.first()) {
                is ContentAudioCapture, is ContentAudioMicrophone -> true
                else -> false
            }
        }

    private val isMicrophoneAndroidPermissionNotGranted: Boolean
        get() {
            return !anchorView.context.isPermissionGranted(RECORD_AUDIO)
        }

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
            permissionFromStorage.camera.isAllowed()
        }
        else ->
            throw InvalidParameterException("$permission is not a valid permission.")
    }
}

@VisibleForTesting
internal fun PermissionRequest.shouldIncludeDoNotAskAgainCheckBox(): Boolean {
    return this.permissions.any { permission ->
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
