/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.view.View
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.support.utils.DefaultSnackbarDelegate
import mozilla.components.support.utils.SnackbarDelegate

class CustomTabContextMenuCandidate {
    companion object {
        /**
         * Returns the default list of context menu candidates for custom tabs/external apps.
         *
         */
        fun defaultCandidates(
            context: Context,
            contextMenuUseCases: ContextMenuUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
        ): List<ContextMenuCandidate> = listOf(
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
    }
}
