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

interface WebsitePermissionInteractor {
    fun onPermissionsShown()
    fun onPermissionToggled(permissionState: WebsitePermission)
}

class WebsitePermissionsView(
    override val containerView: ViewGroup,
    val interactor: WebsitePermissionInteractor
) : LayoutContainer {
    private val context = containerView.context

    val view: View = LayoutInflater.from(context)
        .inflate(R.layout.quicksettings_permissions, containerView, true)

    fun update(state: WebsitePermissionsState) {
        if (state.isVisible) {
            interactor.onPermissionsShown()
        }

        // If more permissions are added into this View we can display them into a list
        // and also use DiffUtil to only update one item in case of a permission change
        bindPermission(state.camera,
                Pair(view.findViewById(R.id.cameraLabel), view.findViewById(R.id.camerStatus)))
        bindPermission(state.location,
                Pair(view.findViewById(R.id.locationLabel), view.findViewById(R.id.locationStatus)))
        bindPermission(state.microphone,
                Pair(view.findViewById(R.id.microphoneLabel), view.findViewById(R.id.microphoneStatus)))
        bindPermission(state.notification,
                Pair(view.findViewById(R.id.notificationLabel), view.findViewById(R.id.notificationStatus)))
    }

    private fun bindPermission(permissionState: WebsitePermission, permissionViews: Pair<TextView, TextView>) {
        val (label, status) = permissionViews

        status.text = permissionState.status
        label.isEnabled = permissionState.isEnabled
        label.isVisible = permissionState.isVisible
        status.isVisible = permissionState.isVisible
        status.setOnClickListener { interactor.onPermissionToggled(permissionState) }
    }
}
