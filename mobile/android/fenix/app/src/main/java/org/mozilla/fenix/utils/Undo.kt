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
import android.app.AlertDialog
import org.mozilla.fenix.R
import android.content.Context
import android.view.accessibility.AccessibilityManager
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
 * @param anchorView A [View] to which [FenixSnackbar] should be anchored.
 */
@Suppress("LongParameterList")
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

    fun showUndoDialog() {
        val dialogBuilder = AlertDialog.Builder(view.context)
        dialogBuilder.setMessage(message).setCancelable(false)
            .setPositiveButton(R.string.a11y_dialog_deleted_confirm) { _, _ ->
            launch {
                operation.invoke()
            }
        }.setNegativeButton(R.string.a11y_dialog_deleted_undo) { _, _ ->
            launch {
                onCancel.invoke()
            }
        }
        val alert = dialogBuilder.create()
        alert.show()
    }

    fun showUndoSnackbar() {
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

    //  It is difficult to use our Snackbars quickly enough with
    //  Talkback enabled, so in that case we show a dialog instead
    if (touchExplorationEnabled(view)) {
        showUndoDialog()
    } else {
        showUndoSnackbar()
    }
}

fun touchExplorationEnabled(view: View): Boolean {
    val am = view.context.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
    return am.isTouchExplorationEnabled
}
