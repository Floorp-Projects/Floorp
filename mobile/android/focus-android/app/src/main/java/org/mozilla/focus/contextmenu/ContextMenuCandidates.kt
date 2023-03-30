/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.contextmenu

import android.content.Context
import android.view.View
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.ui.widgets.DefaultSnackbarDelegate
import mozilla.components.ui.widgets.SnackbarDelegate

object ContextMenuCandidates {
    @Suppress("LongParameterList", "UndocumentedPublicFunction")
    fun get(
        context: Context,
        tabsUseCases: TabsUseCases,
        contextMenuUseCases: ContextMenuUseCases,
        appLinksUseCases: AppLinksUseCases,
        snackBarParentView: View,
        snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
        isCustomTab: Boolean,
    ): List<ContextMenuCandidate> {
        return if (isCustomTab) {
            // the context menu candidates list is the same as in a Fenix custom tab.
            listOf(
                ContextMenuCandidate.createCopyLinkCandidate(
                    context,
                    snackBarParentView,
                    snackbarDelegate,
                ),
                ContextMenuCandidate.createShareLinkCandidate(context),
                ContextMenuCandidate.createSaveImageCandidate(context, contextMenuUseCases),
                ContextMenuCandidate.createSaveVideoAudioCandidate(context, contextMenuUseCases),
                ContextMenuCandidate.createCopyImageLocationCandidate(
                    context,
                    snackBarParentView,
                    snackbarDelegate,
                ),
            )
        } else {
            listOf(
                ContextMenuCandidate.createOpenInPrivateTabCandidate(
                    context,
                    tabsUseCases,
                    snackBarParentView,
                    snackbarDelegate,
                ),
                ContextMenuCandidate.createCopyLinkCandidate(
                    context,
                    snackBarParentView,
                    snackbarDelegate,
                ),
                ContextMenuCandidate.createDownloadLinkCandidate(context, contextMenuUseCases),
                ContextMenuCandidate.createShareLinkCandidate(context),
                ContextMenuCandidate.createShareImageCandidate(context, contextMenuUseCases),
                ContextMenuCandidate.createOpenImageInNewTabCandidate(
                    context,
                    tabsUseCases,
                    snackBarParentView,
                    snackbarDelegate,
                ),
                ContextMenuCandidate.createSaveImageCandidate(context, contextMenuUseCases),
                ContextMenuCandidate.createSaveVideoAudioCandidate(context, contextMenuUseCases),
                ContextMenuCandidate.createCopyImageLocationCandidate(
                    context,
                    snackBarParentView,
                    snackbarDelegate,
                ),
                ContextMenuCandidate.createAddContactCandidate(context),
                ContextMenuCandidate.createShareEmailAddressCandidate(context),
                ContextMenuCandidate.createCopyEmailAddressCandidate(
                    context,
                    snackBarParentView,
                    snackbarDelegate,
                ),
                ContextMenuCandidate.createOpenInExternalAppCandidate(
                    context,
                    appLinksUseCases,
                ),
            )
        }
    }
}
