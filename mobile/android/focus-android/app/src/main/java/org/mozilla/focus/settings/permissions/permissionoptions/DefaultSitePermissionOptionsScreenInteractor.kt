/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings.permissions.permissionoptions

import org.mozilla.focus.settings.permissions.SitePermissionOption

class DefaultSitePermissionOptionsScreenInteractor(
    private val sitePermissionOptionsScreenStore: SitePermissionOptionsScreenStore,
) {
    fun handleSitePermissionOptionSelected(sitePermissionOption: SitePermissionOption) {
        if (sitePermissionOptionsScreenStore.state.selectedSitePermissionOption == sitePermissionOption) {
            return
        }
        sitePermissionOptionsScreenStore.dispatch(
            SitePermissionOptionsScreenAction.Select(
                selectedSitePermissionOption = sitePermissionOption,
            ),
        )
    }
}
