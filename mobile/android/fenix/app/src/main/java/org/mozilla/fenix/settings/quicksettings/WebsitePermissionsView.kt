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
import kotlinx.android.synthetic.main.quicksettings_permissions.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.PhoneFeature
import java.util.EnumMap

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

    private val permissionViews: Map<PhoneFeature, PermissionViewHolder> = EnumMap(mapOf(
        PhoneFeature.CAMERA to PermissionViewHolder(view.cameraLabel, view.cameraStatus),
        PhoneFeature.LOCATION to PermissionViewHolder(view.locationLabel, view.locationStatus),
        PhoneFeature.MICROPHONE to PermissionViewHolder(view.microphoneLabel, view.microphoneStatus),
        PhoneFeature.NOTIFICATION to PermissionViewHolder(view.notificationLabel, view.notificationStatus)
    ))

    /**
     * Allows changing what this View displays.
     *
     * @param state [WebsitePermissionsState] to be rendered.
     */
    fun update(state: WebsitePermissionsState) {
        val isVisible = permissionViews.keys
            .map { feature -> state.getValue(feature) }
            .any { it.isVisible }
        if (isVisible) {
            interactor.onPermissionsShown()
        }

        // If more permissions are added into this View we can display them into a list
        // and also use DiffUtil to only update one item in case of a permission change
        for ((feature, views) in permissionViews) {
            bindPermission(state.getValue(feature), views)
        }
    }

    /**
     * Helper method that can map a specific website permission to a dedicated permission row
     * which will display permission's [icon, label, status] and register user inputs.
     *
     * @param permissionState [WebsitePermission] specific permission that can be shown to the user.
     * @param viewHolder Views that will render [WebsitePermission]'s state.
     */
    private fun bindPermission(permissionState: WebsitePermission, viewHolder: PermissionViewHolder) {
        viewHolder.label.isEnabled = permissionState.isEnabled
        viewHolder.label.isVisible = permissionState.isVisible
        viewHolder.status.text = permissionState.status
        viewHolder.status.isVisible = permissionState.isVisible
        viewHolder.status.setOnClickListener { interactor.onPermissionToggled(permissionState) }
    }

    data class PermissionViewHolder(val label: TextView, val status: TextView)
}
