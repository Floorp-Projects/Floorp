/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import mozilla.components.lib.state.Action
import org.mozilla.fenix.settings.PhoneFeature

/**
 * Parent [Action] for all the [QuickSettingsFragmentState] changes.
 */
sealed class QuickSettingsFragmentAction : Action

/**
 * All possible [WebsiteInfoState] changes as result of user / system interactions.
 */
sealed class WebsiteInfoAction : QuickSettingsFragmentAction()

/**
 * All possible [WebsitePermissionsState] changes as result of user / system interactions.
 */
sealed class WebsitePermissionAction : QuickSettingsFragmentAction() {
    /**
     * Change resulting from toggling a specific [WebsitePermission] for the current website.
     *
     * @param updatedFeature [PhoneFeature] backing a certain [WebsitePermission].
     * Allows to easily identify which permission changed
     * **Must be the name of one of the properties of [WebsitePermissionsState]**.
     * @param updatedStatus [String] the new [WebsitePermission#status] which will be shown to the user.
     * @param updatedEnabledStatus [Boolean] the new [WebsitePermission#enabled] which will be shown to the user.
     */
    class TogglePermission(
        val updatedFeature: PhoneFeature,
        val updatedStatus: String,
        val updatedEnabledStatus: Boolean
    ) : WebsitePermissionAction()
}
