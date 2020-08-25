/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.view.View
import androidx.appcompat.widget.ContentFrameLayout
import androidx.core.view.updatePadding
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.settings
import java.util.concurrent.atomic.AtomicBoolean

internal const val UNDO_DELAY = 3000L
internal const val ACCESSIBLE_UNDO_DELAY = 15000L

/**
 * Runs [operation] after giving user time (see [UNDO_DELAY]) to cancel it.
 * In case of cancellation, [onCancel] is executed.
 *
 * Execution of suspend blocks happens on [Dispatchers.Main].
 *
 * @param view A [View] used to determine a parent for the [FenixSnackbar].
 * @param message A message displayed as part of [FenixSnackbar].
 * @param undoActionTitle Label for the action associated with the [FenixSnackbar].
 * @param onCancel A suspend block to execute in case of cancellation.
 * @param operation A suspend block to execute if user doesn't cancel via the displayed [FenixSnackbar].
 * @param anchorView A [View] to which [FenixSnackbar] should be anchored.
 */
@Suppress("LongParameterList")
fun CoroutineScope.allowUndo(
    view: View,
    message: String,
    undoActionTitle: String,
    onCancel: suspend () -> Unit = {},
    operation: suspend () -> Unit,
    anchorView: View? = null,
    elevation: Float? = null,
    paddedForBottomToolbar: Boolean = false
) {
    // By using an AtomicBoolean, we achieve memory effects of reading and
    // writing a volatile variable.
    val requestedUndo = AtomicBoolean(false)

    fun showUndoSnackbar() {
        val snackbar = FenixSnackbar
            .make(
                view = view,
                duration = FenixSnackbar.LENGTH_INDEFINITE,
                isDisplayedWithBrowserToolbar = false
            )
            .setText(message)
            .setAnchorView(anchorView)
            .setAction(undoActionTitle) {
                requestedUndo.set(true)
                launch {
                    onCancel.invoke()
                }
            }

        elevation?.also {
            snackbar.view.elevation = it
        }

        val shouldUseBottomToolbar = view.context.settings().shouldUseBottomToolbar
        val toolbarHeight = view.context.resources
            .getDimensionPixelSize(R.dimen.browser_toolbar_height)

        snackbar.view.updatePadding(
            bottom = if (
                paddedForBottomToolbar &&
                shouldUseBottomToolbar &&
                // If the view passed in is a ContentFrameLayout, it does not matter
                // if the user has a dynamicBottomToolbar or not, as the Android system
                // can't intelligently position the snackbar on the upper most view.
                // Ideally we should not pass ContentFrameLayout in, but it's the only
                // way to display snackbars through a fragment transition.
                view is ContentFrameLayout
            ) {
                toolbarHeight
            } else {
                0
            }
        )

        snackbar.show()

        // Wait a bit, and if user didn't request cancellation, proceed with
        // requested operation and hide the snackbar.
        launch {
            val lengthToDelay = if (view.context.settings().accessibilityServicesEnabled) {
                ACCESSIBLE_UNDO_DELAY
            } else {
                UNDO_DELAY
            }

            delay(lengthToDelay)

            if (!requestedUndo.get()) {
                snackbar.dismiss()
                operation.invoke()
            }
        }
    }

    showUndoSnackbar()
}
