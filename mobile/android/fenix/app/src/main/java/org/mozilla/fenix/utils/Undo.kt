/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.view.View
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.mozilla.fenix.components.FenixSnackbar
import java.util.concurrent.atomic.AtomicBoolean

internal const val UNDO_DELAY = 3000L

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
 */
fun CoroutineScope.allowUndo(
    view: View,
    message: String,
    undoActionTitle: String,
    onCancel: suspend () -> Unit = {},
    operation: suspend () -> Unit,
    anchorView: View? = null
) {
    // By using an AtomicBoolean, we achieve memory effects of reading and
    // writing a volatile variable.
    val requestedUndo = AtomicBoolean(false)

    // Launch an indefinite snackbar.
    val snackbar = FenixSnackbar
        .make(view, FenixSnackbar.LENGTH_INDEFINITE)
        .setText(message)
        .setAnchorView(anchorView)
        .setAction(undoActionTitle) {
            requestedUndo.set(true)
            launch {
                onCancel.invoke()
            }
        }

    // If user engages with the snackbar, it'll get automatically dismissed.
    snackbar.show()

    // Wait a bit, and if user didn't request cancellation, proceed with
    // requested operation and hide the snackbar.
    launch {
        delay(UNDO_DELAY)

        if (!requestedUndo.get()) {
            snackbar.dismiss()
            operation.invoke()
        }
    }
}
