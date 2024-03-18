/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions.permissionoptions

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

class SitePermissionOptionsStorageMiddleware(
    val sitePermission: SitePermission,
    val storage: SitePermissionOptionsStorage,
) :
    Middleware<SitePermissionOptionsScreenState, SitePermissionOptionsScreenAction> {

    override fun invoke(
        context: MiddlewareContext<SitePermissionOptionsScreenState, SitePermissionOptionsScreenAction>,
        next: (SitePermissionOptionsScreenAction) -> Unit,
        action: SitePermissionOptionsScreenAction,
    ) {
        when (action) {
            is SitePermissionOptionsScreenAction.Select -> {
                storage.saveCurrentSitePermissionOptionInSharePref(
                    action.selectedSitePermissionOption,
                    sitePermission = sitePermission,
                )
                next(action)
            }
            is SitePermissionOptionsScreenAction.InitSitePermissionOptions -> {
                context.dispatch(
                    SitePermissionOptionsScreenAction.UpdateSitePermissionOptions(
                        storage.getSitePermissionOptions(sitePermission),
                        storage.permissionSelectedOption(sitePermission),
                        storage.getSitePermissionLabel(sitePermission),
                        storage.isAndroidPermissionGranted(sitePermission),
                    ),
                )
            }
            else -> {
                next(action)
            }
        }
    }
}
