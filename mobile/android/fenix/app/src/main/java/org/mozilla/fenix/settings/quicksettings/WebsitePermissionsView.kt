/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.view.isVisible
import kotlinx.android.extensions.LayoutContainer
import org.mozilla.fenix.R

/**
 *  Contract declaring all possible user interactions with [WebsitePermissionsView]
 */
interface WebsitePermissionInteractor {
    /**
     * Indicates there are website permissions allowed / blocked for the current website.
     * which, status which is shown to the user.
     */
    fun onPermissionsShown()

    /**
     * Indicates the user changed the status of a certain website permission.
     *
     * @param permissionState current [WebsitePermission] that the user wants toggled.
     */
    fun onPermissionToggled(permissionState: WebsitePermission)
}

/**
 * MVI View that knows to display a list of specific website permissions (hardcoded):
 * - location
 * - notification
 * - microphone
 * - camera
 *
 * @param containerView [ViewGroup] in which this View will inflate itself.
 * @param interactor [WebsitePermissionInteractor] which will have delegated to all user interactions.
 */
class WebsitePermissionsView(
    override val containerView: ViewGroup,
    val interactor: WebsitePermissionInteractor
) : LayoutContainer {
    private val context = containerView.context

    val view: View = LayoutInflater.from(context)
        .inflate(R.layout.quicksettings_permissions, containerView, true)

    /**
     * Allows changing what this View displays.
     *
     * @param state [WebsitePermissionsState] to be rendered.
     */
    fun update(state: WebsitePermissionsState) {
        val isAnyPermissionVisible = state.camera.isVisible || state.location.isVisible ||
                state.microphone.isVisible || state.notification.isVisible

        // Can not use state.isVisible because we are not handling the audio permissions here right
        // now. If we add more permissions below we should update isAnyPermissionVisible too
        if (isAnyPermissionVisible) {
            interactor.onPermissionsShown()
        }

        // If more permissions are added into this View we can display them into a list
        // and also use DiffUtil to only update one item in case of a permission change
        bindPermission(state.camera,
                Pair(view.findViewById(R.id.cameraLabel), view.findViewById(R.id.cameraStatus)))
        bindPermission(state.location,
                Pair(view.findViewById(R.id.locationLabel), view.findViewById(R.id.locationStatus)))
        bindPermission(state.microphone,
                Pair(view.findViewById(R.id.microphoneLabel), view.findViewById(R.id.microphoneStatus)))
        bindPermission(state.notification,
                Pair(view.findViewById(R.id.notificationLabel), view.findViewById(R.id.notificationStatus)))
    }

    /**
     * Helper method that can map a specific website permission to a dedicated permission row
     * which will display permission's [icon, label, status] and register user inputs.
     *
     * @param permissionState [WebsitePermission] specific permission that can be shown to the user.
     * @param permissionViews Views that will render [WebsitePermission]'s state.
     */
    private fun bindPermission(permissionState: WebsitePermission, permissionViews: Pair<TextView, TextView>) {
        val (label, status) = permissionViews

        status.text = permissionState.status
        label.isEnabled = permissionState.isEnabled
        label.isVisible = permissionState.isVisible
        status.isVisible = permissionState.isVisible
        status.setOnClickListener { interactor.onPermissionToggled(permissionState) }
    }
}
