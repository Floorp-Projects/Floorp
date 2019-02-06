/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.content.pm.PackageManager
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.support.base.feature.LifecycleAwareFeature

typealias OnNeedToRequestPermissions = (permissions: Array<String>) -> Unit

internal const val FRAGMENT_TAG = "mozac_feature_site_permissions"

/**
 * This feature will subscribe to the currently selected [Session] and display
 * a suitable dialogs based on [Session.Observer.onAppPermissionRequested] or
 * [Session.Observer.onContentPermissionRequested]  events.
 * Once the dialog is closed the [PermissionRequest] will be consumed.
 *
 * @property sessionManager the [SessionManager] instance in order to subscribe
 * to the selected [Session].
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested. Once the request is completed, [onPermissionsResult] needs to be invoked.
 **/
class SitePermissionsFeature(
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
     * @param sessionId this is the id of the session which requested the permissions.
     * @param url the url which requested the permissions.
     * @param permissions the list of [permissions] that have been granted.
     */
    fun onContentPermissionGranted(sessionId: String, url: String, permissions: List<Permission>) {
        sessionManager.findSessionById(sessionId)?.apply {
            contentPermissionRequest.consume {
                it.grant()
                // Update the DB
                true
            }
        }
    }

    /**
     * Notifies that the permissions requested by this [sessionId] were rejected.
     *
     * @param sessionId this is the id of the session which requested the permissions.
     * @param url the url which requested the permissions.
     */
    fun onContentPermissionDeny(sessionId: String, url: String) {
        sessionManager.findSessionById(sessionId)?.apply {
            contentPermissionRequest.consume {
                it.reject()
                // Update the DB
                true
            }
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

        override fun onAppPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean {
            return feature.onAppPermissionRequested(permissionRequest)
        }
    }
}
