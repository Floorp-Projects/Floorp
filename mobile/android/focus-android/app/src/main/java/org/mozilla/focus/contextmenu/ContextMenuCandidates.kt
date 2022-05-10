/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.contextmenu

import android.content.Context
import android.view.View
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.contextmenu.DefaultSnackbarDelegate
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.focus.nimbus.FocusNimbus

object ContextMenuCandidates {
    @Suppress("LongParameterList")
    fun get(
        context: Context,
        tabsUseCases: TabsUseCases,
        contextMenuUseCases: ContextMenuUseCases,
        appLinksUseCases: AppLinksUseCases,
        snackBarParentView: View,
        snackbarDelegate: ContextMenuCandidate.SnackbarDelegate = DefaultSnackbarDelegate()
    ): List<ContextMenuCandidate> {
        val multiTabsFeature = FocusNimbus.features.tabs
        val multiTabsConfig = multiTabsFeature.value()
        return listOf(
            ContextMenuCandidate.createOpenInPrivateTabCandidate(
                context,
                tabsUseCases,
                snackBarParentView,
                snackbarDelegate,
                additionalValidation = { _: SessionState, _: HitResult ->
                    multiTabsFeature.recordExposure()
                    multiTabsConfig.isMultiTab
                }
            ),
            ContextMenuCandidate.createCopyLinkCandidate(
                context,
                snackBarParentView,
                snackbarDelegate
            ),
            ContextMenuCandidate.createDownloadLinkCandidate(context, contextMenuUseCases),
            ContextMenuCandidate.createShareLinkCandidate(context),
            ContextMenuCandidate.createShareImageCandidate(context, contextMenuUseCases),
            ContextMenuCandidate.createOpenImageInNewTabCandidate(
                context,
                tabsUseCases,
                snackBarParentView,
                snackbarDelegate,
                additionalValidation = { _: SessionState, _: HitResult ->
                    multiTabsFeature.recordExposure()
                    multiTabsConfig.isMultiTab
                }
            ),
            ContextMenuCandidate.createSaveImageCandidate(context, contextMenuUseCases),
            ContextMenuCandidate.createSaveVideoAudioCandidate(context, contextMenuUseCases),
            ContextMenuCandidate.createCopyImageLocationCandidate(
                context,
                snackBarParentView,
                snackbarDelegate
            ),
            ContextMenuCandidate.createAddContactCandidate(context),
            ContextMenuCandidate.createShareEmailAddressCandidate(context),
            ContextMenuCandidate.createCopyEmailAddressCandidate(
                context,
                snackBarParentView,
                snackbarDelegate
            ),
            ContextMenuCandidate.createOpenInExternalAppCandidate(
                context,
                appLinksUseCases
            )
        )
    }
}
