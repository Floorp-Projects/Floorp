/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.share

import android.content.Context
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.support.ktx.android.content.share

/**
 * Delegate to display a share prompt.
 */
interface ShareDelegate {

    /**
     * Displays a share sheet for the given [ShareData].
     *
     * @param context Reference to context.
     * @param shareData Data to share.
     * @param onDismiss Callback to be invoked if the share sheet is dismissed and nothing
     * is selected, or if it fails to load.
     * @param onSuccess Callback to be invoked if the data is successfully shared.
     */
    fun showShareSheet(
        context: Context,
        shareData: ShareData,
        onDismiss: () -> Unit,
        onSuccess: () -> Unit,
    )
}

/**
 * Default [ShareDelegate] implementation that displays the native share sheet.
 */
class DefaultShareDelegate : ShareDelegate {

    override fun showShareSheet(
        context: Context,
        shareData: ShareData,
        onDismiss: () -> Unit,
        onSuccess: () -> Unit,
    ) {
        val shareSucceeded = context.share(
            text = listOfNotNull(shareData.url, shareData.text).joinToString(" "),
            subject = shareData.title.orEmpty(),
        )

        if (shareSucceeded) onSuccess() else onDismiss()
    }
}
