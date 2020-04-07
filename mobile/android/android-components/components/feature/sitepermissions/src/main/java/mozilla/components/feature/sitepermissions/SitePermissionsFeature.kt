/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.Manifest.permission.CAMERA
import android.Manifest.permission.RECORD_AUDIO
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.core.net.toUri
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSession
import mozilla.components.browser.session.runWithSessionIdOrSelected
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentAudioMicrophone
import mozilla.components.concept.engine.permission.Permission.ContentAutoPlayAudible
import mozilla.components.concept.engine.permission.Permission.ContentAutoPlayInaudible
import mozilla.components.concept.engine.permission.Permission.ContentGeoLocation
import mozilla.components.concept.engine.permission.Permission.ContentNotification
import mozilla.components.concept.engine.permission.Permission.ContentVideoCamera
import mozilla.components.concept.engine.permission.Permission.ContentVideoCapture
import mozilla.components.concept.engine.permission.Permission.AppLocationCoarse
import mozilla.components.concept.engine.permission.Permission.AppLocationFine
import mozilla.components.concept.engine.permission.Permission.AppAudio
import mozilla.components.concept.engine.permission.Permission.AppCamera
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.feature.sitepermissions.SitePermissions.Status.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissionsFeature.DialogConfig
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.ktx.android.content.isPermissionGranted
import java.lang.IllegalStateException
import java.security.InvalidParameterException

internal const val FRAGMENT_TAG = "mozac_feature_sitepermissions_prompt_dialog"

/**
 * This feature will subscribe to the currently selected [Session] and display
 * a suitable dialogs based on [Session.Observer.onAppPermissionRequested] or
 * [Session.Observer.onContentPermissionRequested]  events.
 * Once the dialog is closed the [PermissionRequest] will be consumed.
 *
 * @property context a reference to the context.
 * @property sessionManager the [SessionManager] instance in order to subscribe
 * to the selected [Session].
 * @property sessionId optional sessionId to be observed if null the selected session will be observed.
 * @property storage the object in charge of persisting all the [SitePermissions] objects.
 * @property sitePermissionsRules indicates how permissions should behave per permission category.
 * @property fragmentManager a reference to a [FragmentManager], used to show permissions prompts.
 * @property promptsStyling optional styling for prompts.
 * @property dialogConfig optional customization for dialog initial state. See [DialogConfig].
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested. Once the request is completed, [onPermissionsResult] needs to be invoked.
 * @property onShouldShowRequestPermissionRationale a callback that allows the feature to query
 * the ActivityCompat.shouldShowRequestPermissionRationale or the Fragment.shouldShowRequestPermissionRationale values.
 **/

@Suppress("TooManyFunctions", "LargeClass")
class SitePermissionsFeature(
    private val context: Context,
    private val sessionManager: SessionManager,
    private var sessionId: String? = null,
    private val storage: SitePermissionsStorage = SitePermissionsStorage(context),
    var sitePermissionsRules: SitePermissionsRules? = null,
    private val fragmentManager: FragmentManager,
    var promptsStyling: PromptsStyling? = null,
    private val dialogConfig: DialogConfig? = null,
    override val onNeedToRequestPermissions: OnNeedToRequestPermissions,
    val onShouldShowRequestPermissionRationale: (permission: String) -> Boolean
) : LifecycleAwareFeature, PermissionsFeature {

    private val observer = SitePermissionsRequestObserver(sessionManager, feature = this)
    internal val ioCoroutineScope by lazy { coroutineScopeInitializer() }

    internal var coroutineScopeInitializer = {
        CoroutineScope(Dispatchers.IO)
    }

    override fun start() {
        observer.observeIdOrSelected(sessionId)
        fragmentManager.findFragmentByTag(FRAGMENT_TAG)?.let { fragment ->
            // There's still a [SitePermissionsDialogFragment] visible from the last time. Re-attach
            // this feature so that the fragment can invoke the callback on this feature once the user
            // makes a selection. This can happen when the app was in the background and on resume
            // the activity and fragments get recreated.
            reattachFragment(fragment as SitePermissionsDialogFragment)
        }
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
    @Suppress("MaxLineLength")
    override fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        sessionManager.runWithSessionIdOrSelected(sessionId) { session ->
            session.appPermissionRequest.consume { permissionsRequest ->

                val allPermissionWereGranted = grantResults.all { grantResult ->
                    grantResult == PackageManager.PERMISSION_GRANTED
                }

                if (grantResults.isNotEmpty() && allPermissionWereGranted) {
                    permissionsRequest.grant()
                } else {
                    permissionsRequest.reject()
                    permissions.forEach { systemPermission ->
                        if (!onShouldShowRequestPermissionRationale(systemPermission)) {
                            // The system permission is denied permanently
                            val appPermission = listOf(permissionsRequest.permissions.find { it.id == systemPermission }
                                    ?: throw IllegalStateException("$systemPermission is not part of the permission request"))
                            storeSitePermissions(session, permissionsRequest, appPermission, status = BLOCKED)
                        }
                    }
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
    internal fun onContentPermissionGranted(
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

    internal fun onPositiveButtonPress(sessionId: String, shouldStore: Boolean) {
        sessionManager.runWithSession(sessionId) { session ->
            session.contentPermissionRequest.consume {
                onContentPermissionGranted(session, it.permissions, shouldStore)
                true
            }
        }
    }

    internal fun onNegativeButtonPress(sessionId: String, shouldStore: Boolean) {
        sessionManager.runWithSession(sessionId) { session ->
            session.contentPermissionRequest.consume {
                onContentPermissionDeny(session, shouldStore)
                true
            }
        }
    }

    internal fun onDismiss(sessionId: String) {
        sessionManager.runWithSession(sessionId) { session ->
            session.contentPermissionRequest.consume {
                onContentPermissionDeny(session, false)
                true
            }
        }
    }

    internal fun storeSitePermissions(
        session: Session,
        request: PermissionRequest,
        permissions: List<Permission> = request.permissions,
        status: SitePermissions.Status
    ) {
        if (session.private) {
            return
        }
        ioCoroutineScope.launch {
            synchronized(storage) {
                var sitePermissions = storage.findSitePermissionsBy(request.getHost(session))

                if (sitePermissions == null) {
                    sitePermissions = request.toSitePermissions(session, status = status, permissions = permissions)
                    storage.save(sitePermissions)
                } else {
                    sitePermissions = request.toSitePermissions(session, status, sitePermissions)
                    storage.update(sitePermissions)
                }
            }
        }
    }

    /**
     * Notifies that the permissions requested by this [session] were rejected.
     *
     * @param session the session which requested the permissions.
     * @param shouldStore indicates weather the permission should be stored.
     */
    internal fun onContentPermissionDeny(session: Session, shouldStore: Boolean) {
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
    ): SitePermissionsDialogFragment? {

        // We want to warranty that all media permissions have the required system
        // permissions are granted first, otherwise, we reject the request
        if (request.isMedia && !request.areAllMediaPermissionsGranted) {
            request.reject()
            session.contentPermissionRequest.consume { true }
            return null
        }

        val permissionFromStorage = withContext(ioCoroutineScope.coroutineContext) {
            storage.findSitePermissionsBy(request.getHost(session))
        }

        val prompt = if (shouldApplyRules(permissionFromStorage) ||
            request.isForAutoplay()) {
            handleRuledFlow(request, session)
        } else {
            handleNoRuledFlow(permissionFromStorage, request, session)
        }
        prompt?.show(fragmentManager, FRAGMENT_TAG)
        return prompt
    }

    private fun handleNoRuledFlow(
        permissionFromStorage: SitePermissions?,
        permissionRequest: PermissionRequest,
        session: Session
    ): SitePermissionsDialogFragment? {
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

    private fun handleRuledFlow(
        permissionRequest: PermissionRequest,
        session: Session
    ): SitePermissionsDialogFragment? {
        // For now we only support autoplay via sitePermissionsRules until we add support for
        // autoplay for specific sites see Fenix issue  #8603
        return if (permissionRequest.isForAutoplay() && sitePermissionsRules == null) {
            permissionRequest.reject()
            session.contentPermissionRequest.consume { true }
            null
        } else {
            val action = requireNotNull(sitePermissionsRules).getActionFrom(permissionRequest)
            when (action) {
                SitePermissionsRules.Action.ALLOWED -> {
                    permissionRequest.grant()
                    session.contentPermissionRequest.consume { true }
                    null
                }
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
        session: Session,
        status: SitePermissions.Status,
        initialSitePermission: SitePermissions = getInitialSitePermissions(session, this),
        permissions: List<Permission> = this.permissions
    ): SitePermissions {
        var sitePermissions = initialSitePermission
        for (permission in permissions) {
            sitePermissions = updateSitePermissionsStatus(status, permission, sitePermissions)
        }
        return sitePermissions
    }

    internal fun getInitialSitePermissions(session: Session, request: PermissionRequest): SitePermissions {
        val rules = sitePermissionsRules
        return rules?.toSitePermissions(request.getHost(session), savedAt = System.currentTimeMillis())
                ?: SitePermissions(request.getHost(session), savedAt = System.currentTimeMillis())
    }

    private fun PermissionRequest.isForAutoplay() =
        this.permissions.any { it is ContentAutoPlayInaudible || it is ContentAutoPlayAudible }

    private fun updateSitePermissionsStatus(
        status: SitePermissions.Status,
        permission: Permission,
        sitePermissions: SitePermissions
    ): SitePermissions {
        return when (permission) {
            is ContentGeoLocation, is AppLocationCoarse, is AppLocationFine -> {
                sitePermissions.copy(location = status)
            }
            is ContentNotification -> {
                sitePermissions.copy(notification = status)
            }
            is ContentAudioCapture, is ContentAudioMicrophone, is AppAudio -> {
                sitePermissions.copy(microphone = status)
            }
            is ContentVideoCamera, is ContentVideoCapture, is AppCamera -> {
                sitePermissions.copy(camera = status)
            }
            is ContentAutoPlayAudible -> {
                sitePermissions.copy(autoplayAudible = status)
            }
            is ContentAutoPlayInaudible -> {
                sitePermissions.copy(autoplayInaudible = status)
            }
            else ->
                throw InvalidParameterException("$permission is not a valid permission.")
        }
    }

    private fun createPrompt(
        permissionRequest: PermissionRequest,
        session: Session
    ): SitePermissionsDialogFragment {
        val host = permissionRequest.getHost(session)
        return if (!permissionRequest.containsVideoAndAudioSources()) {
            val permission = permissionRequest.permissions.first()
            handlingSingleContentPermissions(session.id, permission, host)
        } else {
            createSinglePermissionPrompt(
                context,
                session.id,
                host,
                R.string.mozac_feature_sitepermissions_camera_and_microphone,
                R.drawable.mozac_ic_microphone,
                showDoNotAskAgainCheckBox = true,
                shouldSelectRememberChoice = dialogConfig?.shouldPreselectDoNotAskAgain
                    ?: DialogConfig.DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN
            )
        }
    }

    private fun handlingSingleContentPermissions(
        sessionId: String,
        permission: Permission,
        host: String
    ): SitePermissionsDialogFragment {
        return when (permission) {
            is ContentGeoLocation -> {
                createSinglePermissionPrompt(
                    context,
                    sessionId,
                    host,
                    R.string.mozac_feature_sitepermissions_location_title,
                    R.drawable.mozac_ic_location,
                    showDoNotAskAgainCheckBox = true,
                    shouldSelectRememberChoice = dialogConfig?.shouldPreselectDoNotAskAgain
                        ?: DialogConfig.DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN
                )
            }
            is ContentNotification -> {
                createSinglePermissionPrompt(
                    context,
                    sessionId,
                    host,
                    R.string.mozac_feature_sitepermissions_notification_title,
                    R.drawable.mozac_ic_notification,
                    showDoNotAskAgainCheckBox = false,
                    shouldSelectRememberChoice = false,
                    isNotificationRequest = true
                )
            }
            is ContentAudioCapture, is ContentAudioMicrophone -> {
                createSinglePermissionPrompt(
                    context,
                    sessionId,
                    host,
                    R.string.mozac_feature_sitepermissions_microfone_title,
                    R.drawable.mozac_ic_microphone,
                    showDoNotAskAgainCheckBox = true,
                    shouldSelectRememberChoice = dialogConfig?.shouldPreselectDoNotAskAgain
                        ?: DialogConfig.DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN
                )
            }
            is ContentVideoCamera, is ContentVideoCapture -> {
                createSinglePermissionPrompt(
                    context,
                    sessionId,
                    host,
                    R.string.mozac_feature_sitepermissions_camera_title,
                    R.drawable.mozac_ic_video,
                    showDoNotAskAgainCheckBox = true,
                    shouldSelectRememberChoice = dialogConfig?.shouldPreselectDoNotAskAgain
                        ?: DialogConfig.DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN
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
        sessionId: String,
        host: String,
        @StringRes titleId: Int,
        @DrawableRes iconId: Int,
        showDoNotAskAgainCheckBox: Boolean,
        shouldSelectRememberChoice: Boolean,
        isNotificationRequest: Boolean = false
    ): SitePermissionsDialogFragment {
        val title = context.getString(titleId, host)

        return SitePermissionsDialogFragment.newInstance(
            sessionId,
            title,
            iconId,
            this,
            showDoNotAskAgainCheckBox,
            isNotificationRequest = isNotificationRequest,
            shouldSelectDoNotAskAgainCheckBox = shouldSelectRememberChoice
        )
    }

    private fun onAppPermissionRequested(permissionRequest: PermissionRequest): Boolean {
        val permissions = permissionRequest.permissions.map { it.id ?: "" }
        onNeedToRequestPermissions(permissions.toTypedArray())
        return false
    }

    private fun PermissionRequest.getHost(session: Session) = (uri?.toUri()?.host ?: session.url.toUri().host ?: "")

    private val PermissionRequest.isMedia: Boolean
        get() {
            return when (permissions.first()) {
                is ContentVideoCamera, is ContentVideoCapture,
                is ContentAudioCapture, is ContentAudioMicrophone -> true
                else -> false
            }
        }

    private val PermissionRequest.areAllMediaPermissionsGranted: Boolean
        get() {
            val systemPermissions = mutableListOf<String>()
            permissions.forEach { permission ->
                when (permission) {
                    is ContentVideoCamera, is ContentVideoCapture -> {
                        systemPermissions.add(CAMERA)
                    }
                    is ContentAudioCapture, is ContentAudioMicrophone -> {
                        systemPermissions.add(RECORD_AUDIO)
                    }
                }
            }
            return systemPermissions.all { context.isPermissionGranted((it)) }
        }

    internal class SitePermissionsRequestObserver(
        sessionManager: SessionManager,
        private val feature: SitePermissionsFeature
    ) : SelectionAwareSessionObserver(sessionManager) {

        override fun onContentPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
            runBlocking {
                if (permissionRequest.permissions.all { it.isSupported() }) {
                    feature.onContentPermissionRequested(session, permissionRequest)
                } else {
                    session.contentPermissionRequest.consume { true }
                    permissionRequest.reject()
                }
            }
            return false
        }

        override fun onAppPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
            return feature.onAppPermissionRequested(permissionRequest)
        }
    }

    data class PromptsStyling(
        val gravity: Int,
        val shouldWidthMatchParent: Boolean = false,
        @ColorRes
        val positiveButtonBackgroundColor: Int? = null,
        @ColorRes
        val positiveButtonTextColor: Int? = null
    )

    /**
     * Customization options for feature request dialog
     */
    data class DialogConfig(
        /** Use **true** to pre-select "Do not ask again" checkbox. */
        val shouldPreselectDoNotAskAgain: Boolean = false
    ) {

        companion object {
            /** Default values for [DialogConfig.shouldPreselectDoNotAskAgain] */
            internal const val DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN = false
        }
    }

    /**
     * Re-attaches a fragment that is still visible but not linked to this feature anymore.
     */
    private fun reattachFragment(fragment: SitePermissionsDialogFragment) {
        val session = sessionManager.findSessionById(fragment.sessionId)
        if (session == null ||
            session.contentPermissionRequest.isConsumed() &&
            session.appPermissionRequest.isConsumed()
        ) {
            fragmentManager.beginTransaction()
                .remove(fragment)
                .commitAllowingStateLoss()
            return
        }
        // Re-assign the feature instance so that the fragment can invoke us once the user makes a selection or cancels
        // the dialog.
        fragment.feature = this
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

private fun Permission.isSupported(): Boolean {
    return when (this) {
        is ContentGeoLocation,
        is ContentNotification,
        is ContentAudioCapture, is ContentAudioMicrophone,
        is ContentVideoCamera, is ContentVideoCapture,
        is ContentAutoPlayAudible, is ContentAutoPlayInaudible -> true
        else -> false
    }
}
