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

class WebsitePermissionsView(
    override val containerView: ViewGroup
) : LayoutContainer {
    private val context = containerView.context

    val view: View = LayoutInflater.from(context)
        .inflate(R.layout.quicksettings_permissions, containerView, true)

    fun update(state: WebsitePermissionsState) {
        bindPermission(state.camera,
                Pair(view.findViewById(R.id.cameraIcon), view.findViewById(R.id.cameraActionLabel)))
        bindPermission(state.location,
                Pair(view.findViewById(R.id.locationIcon), view.findViewById(R.id.locationActionLabel)))
        bindPermission(state.microphone,
                Pair(view.findViewById(R.id.microphoneIcon), view.findViewById(R.id.microphoneActionLabel)))
        bindPermission(state.notification,
                Pair(view.findViewById(R.id.notificationIcon), view.findViewById(R.id.notificationActionLabel)))
    }

    private fun bindPermission(permissionState: WebsitePermission, permissionViews: Pair<TextView, TextView>) {
        val (icon, status) = permissionViews

        status.text = permissionState.status
        status.isEnabled = permissionState.enabled
        icon.isVisible = permissionState.visible
        status.isVisible = permissionState.visible
    }
}
