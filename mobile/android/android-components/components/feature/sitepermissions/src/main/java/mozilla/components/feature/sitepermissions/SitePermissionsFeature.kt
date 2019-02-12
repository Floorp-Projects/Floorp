/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.support.v4.content.ContextCompat
import android.view.View
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.ContentGeoLocation
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.ui.doorhanger.DoorhangerPrompt
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
     * Notifies that the list of [permissions] have been granted for the [sessionId] and [url].
     *
     * @param session the session which requested the permissions.
     * @param url the url which requested the permissions.
     * @param permissions the list of [permissions] that have been granted.
     */
    private fun onContentPermissionGranted(session: Session, url: String, permissions: List<Permission>) {
        session.contentPermissionRequest.consume {
            it.grant()
            // Update the DB
            true
        }
    }

    /**
     * Notifies that the permissions requested by this [sessionId] were rejected.
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

        val allowString = context.getString(R.string.mozac_feature_sitepermissions_allow)
        val denyString = context.getString(R.string.mozac_feature_sitepermissions_not_allow)

        val allowButton = DoorhangerPrompt.Button(allowString, true) {
            onContentPermissionGranted(session, url, permissionRequest.permissions)
        }

        val denyButton = DoorhangerPrompt.Button(denyString) {
            onContentPermissionDeny(session, url)
        }

        val buttons = listOf(denyButton, allowButton)

        val prompt = when (permission) {
            is ContentGeoLocation -> {
                createPromptForLocationPermission(context, uriString, buttons)
            }
            else ->
                throw InvalidParameterException("$permission is not a valid permission.")
        }

        prompt.createDoorhanger(context).show(anchorView)
        return prompt
    }

    @SuppressLint("VisibleForTests")
    private fun createPromptForLocationPermission(
        context: Context,
        uriString: String,
        buttons: List<DoorhangerPrompt.Button>
    ): DoorhangerPrompt {
        val title = context.getString(R.string.mozac_feature_sitepermissions_location_title, uriString)
        val drawable = ContextCompat.getDrawable(context, R.drawable.mozac_ic_location)

        return DoorhangerPrompt(
            title = title,
            icon = drawable,
            buttons = buttons
        )
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
