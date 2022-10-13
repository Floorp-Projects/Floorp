/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.Manifest.permission.CAMERA
import android.Manifest.permission.RECORD_AUDIO
import android.content.Context
import android.content.pm.PackageManager
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.AutoPlayAudibleBlockingAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.AutoPlayAudibleChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.AutoPlayInAudibleBlockingAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.AutoPlayInAudibleChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.CameraChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.LocationChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.MediaKeySystemAccesChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.MicrophoneChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.NotificationChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.PersistentStorageChangedAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.AppAudio
import mozilla.components.concept.engine.permission.Permission.AppCamera
import mozilla.components.concept.engine.permission.Permission.AppLocationCoarse
import mozilla.components.concept.engine.permission.Permission.AppLocationFine
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentAudioMicrophone
import mozilla.components.concept.engine.permission.Permission.ContentAutoPlayAudible
import mozilla.components.concept.engine.permission.Permission.ContentAutoPlayInaudible
import mozilla.components.concept.engine.permission.Permission.ContentCrossOriginStorageAccess
import mozilla.components.concept.engine.permission.Permission.ContentGeoLocation
import mozilla.components.concept.engine.permission.Permission.ContentMediaKeySystemAccess
import mozilla.components.concept.engine.permission.Permission.ContentNotification
import mozilla.components.concept.engine.permission.Permission.ContentPersistentStorage
import mozilla.components.concept.engine.permission.Permission.ContentVideoCamera
import mozilla.components.concept.engine.permission.Permission.ContentVideoCapture
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.feature.sitepermissions.SitePermissionsFeature.DialogConfig
import mozilla.components.feature.tabs.TabsUseCases.SelectOrAddUseCase
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.ktx.android.content.isPermissionGranted
import mozilla.components.support.ktx.kotlin.getOrigin
import mozilla.components.support.ktx.kotlin.stripDefaultPort
import mozilla.components.support.ktx.kotlinx.coroutines.flow.filterChanged
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import java.security.InvalidParameterException

internal const val FRAGMENT_TAG = "mozac_feature_sitepermissions_prompt_dialog"

@VisibleForTesting
internal const val STORAGE_ACCESS_DOCUMENTATION_URL =
    "https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API"

/**
 * This feature will collect [PermissionRequest] from [ContentState] and display
 * suitable [SitePermissionsDialogFragment].
 * Once the dialog is closed the [PermissionRequest] will be consumed.
 *
 * @property context a reference to the context.
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
 * @property shouldShowDoNotAskAgainCheckBox optional Visibility for Do not ask again Checkbox
 **/

@Suppress("TooManyFunctions", "LargeClass", "LongParameterList")
class SitePermissionsFeature(
    private val context: Context,
    private var sessionId: String? = null,
    private val storage: SitePermissionsStorage = OnDiskSitePermissionsStorage(context),
    var sitePermissionsRules: SitePermissionsRules? = null,
    private val fragmentManager: FragmentManager,
    var promptsStyling: PromptsStyling? = null,
    private val dialogConfig: DialogConfig? = null,
    override val onNeedToRequestPermissions: OnNeedToRequestPermissions,
    val onShouldShowRequestPermissionRationale: (permission: String) -> Boolean,
    private val store: BrowserStore,
    private val shouldShowDoNotAskAgainCheckBox: Boolean = true,
) : LifecycleAwareFeature, PermissionsFeature {
    @VisibleForTesting
    internal val selectOrAddUseCase by lazy {
        SelectOrAddUseCase(store)
    }

    internal val ioCoroutineScope by lazy { coroutineScopeInitializer() }

    internal var coroutineScopeInitializer = {
        CoroutineScope(Dispatchers.IO)
    }
    private var sitePermissionScope: CoroutineScope? = null
    private var appPermissionScope: CoroutineScope? = null
    private var loadingScope: CoroutineScope? = null

    override fun start() {
        fragmentManager.findFragmentByTag(FRAGMENT_TAG)?.let { fragment ->
            // There's still a [SitePermissionsDialogFragment] visible from the last time. Re-attach
            // this feature so that the fragment can invoke the callback on this feature once the user
            // makes a selection. This can happen when the app was in the background and on resume
            // the activity and fragments get recreated.
            reattachFragment(fragment as SitePermissionsDialogFragment)
        }

        setupPermissionRequestsCollector()
        setupAppPermissionRequestsCollector()
        setupLoadingCollector()
    }

    @VisibleForTesting
    internal fun setupLoadingCollector() {
        loadingScope = store.flowScoped { flow ->
            flow.mapNotNull { state ->
                state.findTabOrCustomTabOrSelectedTab(sessionId)
            }.ifChanged { it.content.loading }.collect { tab ->
                if (tab.content.loading) {
                    // Clears stale permission indicators in the toolbar,
                    // after the session starts loading.
                    store.dispatch(UpdatePermissionHighlightsStateAction.Reset(tab.id))
                    storage.clearTemporaryPermissions()
                }
            }
        }
    }

    @VisibleForTesting
    internal fun setupAppPermissionRequestsCollector() {
        appPermissionScope =
            store.flowScoped { flow ->
                flow.mapNotNull { state ->
                    state.findTabOrCustomTabOrSelectedTab(sessionId)?.content?.appPermissionRequestsList
                }
                    .filterChanged { it }
                    .collect { appPermissionRequest ->
                        val permissions = appPermissionRequest.permissions.map { it.id ?: "" }
                        onNeedToRequestPermissions(permissions.toTypedArray())
                    }
            }
    }

    @VisibleForTesting
    internal fun setupPermissionRequestsCollector() {
        sitePermissionScope =
            store.flowScoped { flow ->
                flow.mapNotNull { state ->
                    state.findTabOrCustomTabOrSelectedTab(sessionId)?.content?.permissionRequestsList
                }
                    .filterChanged { it }
                    .collect { permissionRequest ->
                        val origin: String = permissionRequest.uri?.getOrigin().orEmpty()

                        if (origin.isEmpty()) {
                            permissionRequest.consumeAndReject()
                        } else {
                            if (permissionRequest.permissions.all { it.isSupported() }) {
                                onContentPermissionRequested(
                                    permissionRequest,
                                    origin,
                                )
                            } else {
                                permissionRequest.consumeAndReject()
                            }
                        }
                    }
            }
    }

    private fun PermissionRequest.consumeAndReject() {
        consumePermissionRequest(this)
        this.reject()
    }

    @VisibleForTesting
    internal fun consumePermissionRequest(
        permissionRequest: PermissionRequest,
        optionalSessionId: String? = null,
    ) {
        val thisSessionId = optionalSessionId ?: getCurrentTabState()?.id
        thisSessionId?.let { sessionId ->
            store.dispatch(ContentAction.ConsumePermissionsRequest(sessionId, permissionRequest))
        }
    }

    @VisibleForTesting
    internal fun consumeAppPermissionRequest(
        appPermissionRequest: PermissionRequest,
        optionalSessionId: String? = null,
    ) {
        val thisSessionId = optionalSessionId ?: getCurrentTabState()?.id
        thisSessionId?.let { sessionId ->
            store.dispatch(
                ContentAction.ConsumeAppPermissionsRequest(
                    sessionId,
                    appPermissionRequest,
                ),
            )
        }
    }

    override fun stop() {
        sitePermissionScope?.cancel()
        appPermissionScope?.cancel()
        loadingScope?.cancel()
    }

    /**
     * Notifies the feature that the permissions requested were completed.
     *
     * @param grantResults the grant results for the corresponding permissions
     * @see [onNeedToRequestPermissions].
     */
    @Suppress("NestedBlockDepth")
    override fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        val currentContentSate = getCurrentContentState()
        val appPermissionRequest = findRequestedAppPermission(permissions)

        if (appPermissionRequest != null && currentContentSate != null) {
            val allPermissionWereGranted = grantResults.all { grantResult ->
                grantResult == PackageManager.PERMISSION_GRANTED
            }

            if (grantResults.isNotEmpty() && allPermissionWereGranted) {
                appPermissionRequest.grant()
            } else {
                appPermissionRequest.reject()
                permissions.forEach { systemPermission ->
                    if (!onShouldShowRequestPermissionRationale(systemPermission)) {
                        // The system permission is denied permanently
                        storeSitePermissions(
                            currentContentSate,
                            appPermissionRequest,
                            status = BLOCKED,
                        )
                    }
                }
            }
            consumeAppPermissionRequest(appPermissionRequest)
        }
    }

    @VisibleForTesting
    internal fun getCurrentContentState() = getCurrentTabState()?.content

    @VisibleForTesting
    internal fun getCurrentTabState() = store.state.findTabOrCustomTabOrSelectedTab(sessionId)

    @VisibleForTesting
    internal fun findRequestedAppPermission(permissions: Array<String>): PermissionRequest? {
        return getCurrentContentState()?.appPermissionRequestsList?.find {
            permissions.contains(it.permissions.first().id)
        }
    }

    @VisibleForTesting
    internal fun findRequestedPermission(permissionId: String): PermissionRequest? {
        return getCurrentContentState()?.permissionRequestsList?.find {
            it.id == permissionId
        }
    }

    @VisibleForTesting
    internal fun onContentPermissionGranted(
        permissionRequest: PermissionRequest,
        shouldStore: Boolean,
    ) {
        permissionRequest.grant()
        if (shouldStore) {
            getCurrentContentState()?.let { contentState ->
                storeSitePermissions(contentState, permissionRequest, ALLOWED)
            }
        } else {
            storage.saveTemporary(permissionRequest)
        }
    }

    internal fun onPositiveButtonPress(
        permissionId: String,
        sessionId: String,
        shouldStore: Boolean,
    ) {
        findRequestedPermission(permissionId)?.let { permissionRequest ->
            consumePermissionRequest(permissionRequest, sessionId)
            onContentPermissionGranted(permissionRequest, shouldStore)

            if (!permissionRequest.containsVideoAndAudioSources()) {
                emitPermissionAllowed(permissionRequest.permissions.first())
            } else {
                emitPermissionsAllowed(permissionRequest.permissions)
            }
        }
    }

    internal fun onNegativeButtonPress(
        permissionId: String,
        sessionId: String,
        shouldStore: Boolean,
    ) {
        findRequestedPermission(permissionId)?.let { permissionRequest ->
            consumePermissionRequest(permissionRequest, sessionId)
            onContentPermissionDeny(permissionRequest, shouldStore)

            if (!permissionRequest.containsVideoAndAudioSources()) {
                emitPermissionDenied(permissionRequest.permissions.first())
            } else {
                emitPermissionsDenied(permissionRequest.permissions)
            }
        }
    }

    internal fun onLearnMorePress(
        permissionId: String,
        sessionId: String,
    ) {
        findRequestedPermission(permissionId)?.let { permissionRequest ->
            consumePermissionRequest(permissionRequest, sessionId)
            onContentPermissionDeny(permissionRequest, false)

            val permission = permissionRequest.permissions.first()
            if (permission is ContentCrossOriginStorageAccess) {
                store.state.findTabOrCustomTabOrSelectedTab(sessionId)?.let {
                    selectOrAddUseCase.invoke(
                        url = STORAGE_ACCESS_DOCUMENTATION_URL,
                        private = it.content.private,
                        source = SessionState.Source.Internal.TextSelection,
                    )
                }
            }
        }
    }

    internal fun onDismiss(
        permissionId: String,
        sessionId: String,
    ) {
        findRequestedPermission(permissionId)?.let { permissionRequest ->
            consumePermissionRequest(permissionRequest, sessionId)
            onContentPermissionDeny(permissionRequest, false)
        }
    }

    internal fun storeSitePermissions(
        contentState: ContentState,
        request: PermissionRequest,
        status: SitePermissions.Status,
        coroutineScope: CoroutineScope = ioCoroutineScope,
    ) {
        if (contentState.private) {
            return
        }
        updatePermissionToolbarIndicator(request, status, true)
        coroutineScope.launch {
            request.uri?.getOrigin()?.let { origin ->
                var sitePermissions =
                    storage.findSitePermissionsBy(origin)

                if (sitePermissions == null) {
                    sitePermissions =
                        request.toSitePermissions(
                            origin,
                            status = status,
                            permissions = request.permissions,
                        )
                    storage.save(sitePermissions, request)
                } else {
                    sitePermissions = request.toSitePermissions(origin, status, sitePermissions)
                    storage.update(sitePermissions)
                }
            }
        }
    }

    internal fun onContentPermissionDeny(
        permissionRequest: PermissionRequest,
        shouldStore: Boolean,
    ) {
        permissionRequest.reject()
        if (shouldStore) {
            getCurrentContentState()?.let { contentState ->
                storeSitePermissions(contentState, permissionRequest, BLOCKED)
            }
        } else {
            storage.saveTemporary(permissionRequest)
        }
    }

    internal suspend fun onContentPermissionRequested(
        permissionRequest: PermissionRequest,
        origin: String,
        coroutineScope: CoroutineScope = ioCoroutineScope,
    ): SitePermissionsDialogFragment? {
        // We want to warranty that all media permissions have the required system
        // permissions are granted first, otherwise, we reject the request
        if (permissionRequest.isMedia && !permissionRequest.areAllMediaPermissionsGranted) {
            permissionRequest.reject()
            consumePermissionRequest(permissionRequest)
            return null
        }

        val permissionFromStorage = withContext(coroutineScope.coroutineContext) {
            storage.findSitePermissionsBy(origin)
        }

        val prompt = if (shouldApplyRules(permissionFromStorage)) {
            handleRuledFlow(permissionRequest, origin)
        } else {
            handleNoRuledFlow(permissionFromStorage, permissionRequest, origin)
        }
        prompt?.show(fragmentManager, FRAGMENT_TAG)
        return prompt
    }

    @VisibleForTesting
    internal fun handleNoRuledFlow(
        permissionFromStorage: SitePermissions?,
        permissionRequest: PermissionRequest,
        host: String,
    ): SitePermissionsDialogFragment? {
        return if (shouldShowPrompt(permissionRequest, permissionFromStorage)) {
            createPrompt(permissionRequest, host)
        } else {
            val status = if (permissionFromStorage.isGranted(permissionRequest)) {
                permissionRequest.grant()
                ALLOWED
            } else {
                permissionRequest.reject()
                BLOCKED
            }
            updatePermissionToolbarIndicator(
                permissionRequest,
                status,
                permissionFromStorage != null,
            )
            consumePermissionRequest(permissionRequest)
            null
        }
    }

    @VisibleForTesting
    internal fun shouldShowPrompt(
        permissionRequest: PermissionRequest,
        permissionFromStorage: SitePermissions?,
    ): Boolean {
        return if (permissionRequest.isForAutoplay()) {
            false
        } else {
            (
                permissionFromStorage == null ||
                    !permissionRequest.doNotAskAgain(permissionFromStorage)
                )
        }
    }

    @VisibleForTesting
    internal fun handleRuledFlow(
        permissionRequest: PermissionRequest,
        origin: String,
    ): SitePermissionsDialogFragment? {
        return when (sitePermissionsRules?.getActionFrom(permissionRequest)) {
            SitePermissionsRules.Action.ALLOWED -> {
                permissionRequest.grant()
                consumePermissionRequest(permissionRequest)
                updatePermissionToolbarIndicator(permissionRequest, ALLOWED)
                null
            }
            SitePermissionsRules.Action.BLOCKED -> {
                permissionRequest.reject()
                consumePermissionRequest(permissionRequest)
                updatePermissionToolbarIndicator(permissionRequest, BLOCKED)
                null
            }
            SitePermissionsRules.Action.ASK_TO_ALLOW -> {
                createPrompt(permissionRequest, origin)
            }
            null -> {
                consumePermissionRequest(permissionRequest)
                null
            }
        }
    }

    @VisibleForTesting
    @Suppress("ComplexMethod")
    internal fun updatePermissionToolbarIndicator(
        request: PermissionRequest,
        value: SitePermissions.Status,
        permanent: Boolean = false,
    ) {
        val isAutoPlayAudibleBlocking: Boolean? =
            if (request.isForAutoplayAudible()) value == BLOCKED else null
        val isAutoPlayInAudibleBlocking: Boolean? =
            if (request.isForAutoplayInaudible()) value == BLOCKED else null

        getCurrentTabState()?.let { tab ->
            // At the moment, we don't have APIs for controlling temporary permissions,
            // after they are ALLOWED/BLOCKED, for this reason, we are not notifying users
            // when permissions have changed from their default values,
            // as they are not going have a way to change permissions.
            // Either way, temporary permissions have to be ALLOWED/BLOCKED per sessions
            // users are already aware of them.
            // The autoplay permissions work a bit different, as it is a global permissions,
            // users are never prompt to select its value, and it can't be temporary,
            // they only can change it's value per site persistently.
            if (permanent || request.isForAutoplay()) {
                val action = when {
                    request.isForNotification() -> NotificationChangedAction(
                        tab.id,
                        value != sitePermissionsRules?.notification?.toStatus(),
                    )
                    request.isForCamera() -> CameraChangedAction(
                        tab.id,
                        value != sitePermissionsRules?.camera?.toStatus(),
                    )
                    request.isForLocation() -> LocationChangedAction(
                        tab.id,
                        value != sitePermissionsRules?.location?.toStatus(),
                    )
                    request.isForMicrophone() -> MicrophoneChangedAction(
                        tab.id,
                        value != sitePermissionsRules?.microphone?.toStatus(),
                    )
                    request.isForPersistentStorage() -> PersistentStorageChangedAction(
                        tab.id,
                        value != sitePermissionsRules?.persistentStorage?.toStatus(),
                    )
                    request.isForMediaKeySystemAccess() -> MediaKeySystemAccesChangedAction(
                        tab.id,
                        value != sitePermissionsRules?.mediaKeySystemAccess?.toStatus(),
                    )
                    request.isForAutoplayAudible() -> AutoPlayAudibleChangedAction(
                        tab.id,
                        value != sitePermissionsRules?.autoplayAudible?.toAutoplayStatus()
                            ?.toStatus(),
                    )
                    request.isForAutoplayInaudible() -> AutoPlayInAudibleChangedAction(
                        tab.id,
                        value != sitePermissionsRules?.autoplayInaudible?.toAutoplayStatus()
                            ?.toStatus(),
                    )
                    else -> null
                }
                action?.let {
                    store.dispatch(it)
                }
            }
            isAutoPlayAudibleBlocking?.let {
                store.dispatch(AutoPlayAudibleBlockingAction(tab.id, it))
            }
            isAutoPlayInAudibleBlocking?.let {
                store.dispatch(AutoPlayInAudibleBlockingAction(tab.id, it))
            }
        }
    }

    @VisibleForTesting
    internal fun shouldApplyRules(permissionFromStorage: SitePermissions?) =
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
                is ContentPersistentStorage -> {
                    permissionFromStore.localStorage.doNotAskAgain()
                }
                is ContentMediaKeySystemAccess -> {
                    permissionFromStore.mediaKeySystemAccess.doNotAskAgain()
                }
                is ContentCrossOriginStorageAccess -> {
                    permissionFromStore.crossOriginStorageAccess.doNotAskAgain()
                }
                else -> false
            }
        }
    }

    private fun PermissionRequest.toSitePermissions(
        host: String,
        status: SitePermissions.Status,
        initialSitePermission: SitePermissions = getInitialSitePermissions(host),
        permissions: List<Permission> = this.permissions,
    ): SitePermissions {
        var sitePermissions = initialSitePermission
        for (permission in permissions) {
            sitePermissions = updateSitePermissionsStatus(status, permission, sitePermissions)
        }
        return sitePermissions
    }

    @VisibleForTesting
    internal fun getInitialSitePermissions(
        host: String,
    ): SitePermissions {
        val rules = sitePermissionsRules
        return rules?.toSitePermissions(
            host,
            savedAt = System.currentTimeMillis(),
        )
            ?: SitePermissions(host, savedAt = System.currentTimeMillis())
    }

    private fun PermissionRequest.isForAutoplay() =
        this.permissions.any { it is ContentAutoPlayInaudible || it is ContentAutoPlayAudible }

    private fun PermissionRequest.isForNotification() =
        this.permissions.any { it is ContentNotification }

    private fun PermissionRequest.isForCamera() =
        this.permissions.any {
            it is ContentVideoCamera || it is ContentVideoCapture || it is AppCamera
        }

    private fun PermissionRequest.isForAutoplayInaudible() =
        this.permissions.any { it is ContentAutoPlayInaudible }

    private fun PermissionRequest.isForAutoplayAudible() =
        this.permissions.any { it is ContentAutoPlayAudible }

    private fun PermissionRequest.isForLocation() =
        this.permissions.any {
            it is ContentGeoLocation ||
                it is AppLocationCoarse ||
                it is AppLocationFine
        }

    private fun PermissionRequest.isForMicrophone() =
        this.permissions.any {
            it is ContentAudioCapture || it is ContentAudioMicrophone ||
                it is AppAudio
        }

    private fun PermissionRequest.isForPersistentStorage() =
        this.permissions.any { it is ContentPersistentStorage }

    private fun PermissionRequest.isForMediaKeySystemAccess() =
        this.permissions.any { it is ContentMediaKeySystemAccess }

    @VisibleForTesting
    internal fun updateSitePermissionsStatus(
        status: SitePermissions.Status,
        permission: Permission,
        sitePermissions: SitePermissions,
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
                sitePermissions.copy(autoplayAudible = status.toAutoplayStatus())
            }
            is ContentAutoPlayInaudible -> {
                sitePermissions.copy(autoplayInaudible = status.toAutoplayStatus())
            }
            is ContentPersistentStorage -> {
                sitePermissions.copy(localStorage = status)
            }
            is ContentMediaKeySystemAccess -> {
                sitePermissions.copy(mediaKeySystemAccess = status)
            }
            is ContentCrossOriginStorageAccess -> {
                sitePermissions.copy(crossOriginStorageAccess = status)
            }
            else ->
                throw InvalidParameterException("$permission is not a valid permission.")
        }
    }

    @VisibleForTesting
    internal fun createPrompt(
        permissionRequest: PermissionRequest,
        host: String,
    ): SitePermissionsDialogFragment {
        return if (!permissionRequest.containsVideoAndAudioSources()) {
            val permission = permissionRequest.permissions.first()
            handlingSingleContentPermissions(permissionRequest, permission, host).also {
                emitPermissionDialogDisplayed(permission)
            }
        } else {
            createSinglePermissionPrompt(
                context,
                host,
                permissionRequest,
                R.string.mozac_feature_sitepermissions_camera_and_microphone,
                R.drawable.mozac_ic_microphone,
                showDoNotAskAgainCheckBox = shouldShowDoNotAskAgainCheckBox,
                shouldSelectRememberChoice = dialogConfig?.shouldPreselectDoNotAskAgain
                    ?: DialogConfig.DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN,
            ).also {
                emitPermissionsDialogDisplayed(permissionRequest.permissions)
            }
        }
    }

    @Suppress("LongMethod")
    @VisibleForTesting
    internal fun handlingSingleContentPermissions(
        permissionRequest: PermissionRequest,
        permission: Permission,
        host: String,
    ): SitePermissionsDialogFragment {
        return when (permission) {
            is ContentGeoLocation -> {
                createSinglePermissionPrompt(
                    context,
                    host,
                    permissionRequest,
                    R.string.mozac_feature_sitepermissions_location_title,
                    R.drawable.mozac_ic_location,
                    showDoNotAskAgainCheckBox = shouldShowDoNotAskAgainCheckBox,
                    shouldSelectRememberChoice = dialogConfig?.shouldPreselectDoNotAskAgain
                        ?: DialogConfig.DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN,
                )
            }
            is ContentNotification -> {
                createSinglePermissionPrompt(
                    context,
                    host,
                    permissionRequest,
                    R.string.mozac_feature_sitepermissions_notification_title,
                    R.drawable.mozac_ic_notification,
                    showDoNotAskAgainCheckBox = false,
                    shouldSelectRememberChoice = false,
                    isNotificationRequest = true,
                )
            }
            is ContentAudioCapture, is ContentAudioMicrophone -> {
                createSinglePermissionPrompt(
                    context,
                    host,
                    permissionRequest,
                    R.string.mozac_feature_sitepermissions_microfone_title,
                    R.drawable.mozac_ic_microphone,
                    showDoNotAskAgainCheckBox = shouldShowDoNotAskAgainCheckBox,
                    shouldSelectRememberChoice = dialogConfig?.shouldPreselectDoNotAskAgain
                        ?: DialogConfig.DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN,
                )
            }
            is ContentVideoCamera, is ContentVideoCapture -> {
                createSinglePermissionPrompt(
                    context,
                    host,
                    permissionRequest,
                    R.string.mozac_feature_sitepermissions_camera_title,
                    R.drawable.mozac_ic_video,
                    showDoNotAskAgainCheckBox = shouldShowDoNotAskAgainCheckBox,
                    shouldSelectRememberChoice = dialogConfig?.shouldPreselectDoNotAskAgain
                        ?: DialogConfig.DEFAULT_PRESELECT_DO_NOT_ASK_AGAIN,
                )
            }
            is ContentPersistentStorage -> {
                createSinglePermissionPrompt(
                    context,
                    host,
                    permissionRequest,
                    R.string.mozac_feature_sitepermissions_persistent_storage_title,
                    R.drawable.mozac_ic_storage,
                    showDoNotAskAgainCheckBox = false,
                    shouldSelectRememberChoice = true,
                )
            }
            is ContentMediaKeySystemAccess -> {
                createSinglePermissionPrompt(
                    context,
                    host,
                    permissionRequest,
                    R.string.mozac_feature_sitepermissions_media_key_system_access_title,
                    R.drawable.mozac_ic_link,
                    showDoNotAskAgainCheckBox = false,
                    shouldSelectRememberChoice = true,
                )
            }
            is ContentCrossOriginStorageAccess -> {
                createContentCrossOriginStorageAccessPermissionPrompt(
                    context = context,
                    host = host,
                    permissionRequest = permissionRequest,
                    showDoNotAskAgainCheckBox = false,
                    shouldSelectRememberChoice = true,
                )
            }
            else ->
                throw InvalidParameterException("$permission is not a valid permission.")
        }
    }

    @Suppress("LongParameterList")
    @VisibleForTesting
    internal fun createSinglePermissionPrompt(
        context: Context,
        host: String,
        permissionRequest: PermissionRequest,
        @StringRes titleId: Int,
        @DrawableRes iconId: Int,
        showDoNotAskAgainCheckBox: Boolean,
        shouldSelectRememberChoice: Boolean,
        isNotificationRequest: Boolean = false,
    ): SitePermissionsDialogFragment {
        val title = context.getString(titleId, host)

        val currentSessionId: String = store.state.findTabOrCustomTabOrSelectedTab(sessionId)?.id
            ?: throw IllegalStateException("Unable to find session for $sessionId or selected session")

        return SitePermissionsDialogFragment.newInstance(
            currentSessionId,
            title,
            iconId,
            permissionRequest.id,
            this,
            showDoNotAskAgainCheckBox,
            isNotificationRequest = isNotificationRequest,
            shouldSelectDoNotAskAgainCheckBox = shouldSelectRememberChoice,
        )
    }

    @VisibleForTesting
    internal fun createContentCrossOriginStorageAccessPermissionPrompt(
        context: Context,
        host: String,
        permissionRequest: PermissionRequest,
        showDoNotAskAgainCheckBox: Boolean,
        shouldSelectRememberChoice: Boolean,
    ): SitePermissionsDialogFragment {
        val currentSession = store.state.findTabOrCustomTabOrSelectedTab(sessionId)
            ?: throw IllegalStateException("Unable to find session for $sessionId or selected session")

        val title = context.getString(
            R.string.mozac_feature_sitepermissions_storage_access_title,
            host.stripDefaultPort(),
            currentSession.content.url.stripDefaultPort(),
        )
        val message = context.getString(
            R.string.mozac_feature_sitepermissions_storage_access_message,
            host.stripDefaultPort(),
        )
        val negativeButtonText = context.getString(R.string.mozac_feature_sitepermissions_storage_access_not_allow)

        return SitePermissionsDialogFragment.newInstance(
            sessionId = currentSession.id,
            title = title,
            titleIcon = R.drawable.mozac_ic_cookies,
            message = message,
            negativeButtonText = negativeButtonText,
            permissionRequestId = permissionRequest.id,
            feature = this,
            shouldShowDoNotAskAgainCheckBox = showDoNotAskAgainCheckBox,
            isNotificationRequest = false,
            shouldSelectDoNotAskAgainCheckBox = shouldSelectRememberChoice,
            shouldShowLearnMoreLink = true,
        )
    }

    private val PermissionRequest.isMedia: Boolean
        get() {
            return when (permissions.first()) {
                is ContentVideoCamera, is ContentVideoCapture,
                is ContentAudioCapture, is ContentAudioMicrophone,
                -> true
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
                    else -> {
                        // no-op
                    }
                }
            }
            return systemPermissions.all { context.isPermissionGranted((it)) }
        }

    data class PromptsStyling(
        val gravity: Int,
        val shouldWidthMatchParent: Boolean = false,
        @ColorRes
        val positiveButtonBackgroundColor: Int? = null,
        @ColorRes
        val positiveButtonTextColor: Int? = null,
    )

    /**
     * Customization options for feature request dialog
     */
    data class DialogConfig(
        /** Use **true** to pre-select "Do not ask again" checkbox. */
        val shouldPreselectDoNotAskAgain: Boolean = false,
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
        val currentTab = store.state.findTabOrCustomTabOrSelectedTab(fragment.sessionId)?.content
        if (currentTab == null || (noPermissionRequests(currentTab))) {
            fragmentManager.beginTransaction()
                .remove(fragment)
                .commitAllowingStateLoss()
        } else {
            // Re-assign the feature instance so that the fragment can invoke us once the
            // user makes a selection or cancels the dialog.
            fragment.feature = this
        }
    }

    @VisibleForTesting
    internal fun noPermissionRequests(contentState: ContentState) =
        contentState.appPermissionRequestsList.isEmpty() &&
            contentState.permissionRequestsList.isEmpty()
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

@VisibleForTesting
internal fun isPermissionGranted(
    permission: Permission,
    permissionFromStorage: SitePermissions,
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
        is ContentPersistentStorage -> {
            permissionFromStorage.localStorage.isAllowed()
        }
        is ContentCrossOriginStorageAccess -> {
            permissionFromStorage.crossOriginStorageAccess.isAllowed()
        }
        is ContentMediaKeySystemAccess -> {
            permissionFromStorage.mediaKeySystemAccess.isAllowed()
        }
        is ContentAutoPlayAudible -> {
            permissionFromStorage.autoplayAudible.isAllowed()
        }
        is ContentAutoPlayInaudible -> {
            permissionFromStorage.autoplayInaudible.isAllowed()
        }
        else ->
            throw InvalidParameterException("$permission is not a valid permission.")
    }
}

private fun Permission.isSupported(): Boolean {
    return when (this) {
        is ContentGeoLocation,
        is ContentNotification,
        is ContentPersistentStorage,
        is ContentCrossOriginStorageAccess,
        is ContentAudioCapture, is ContentAudioMicrophone,
        is ContentVideoCamera, is ContentVideoCapture,
        is ContentAutoPlayAudible, is ContentAutoPlayInaudible,
        is ContentMediaKeySystemAccess,
        -> true
        else -> false
    }
}
