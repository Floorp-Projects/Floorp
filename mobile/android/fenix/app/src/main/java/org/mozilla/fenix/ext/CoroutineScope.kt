/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.view.View
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.mozilla.fenix.components.FenixSnackbar

fun CoroutineScope.allowUndo(view: View, message: String, undoActionTitle: String, operation: suspend () -> Unit) {
    val undoJob = launch(Dispatchers.IO) {
        delay(UNDO_DELAY)
        operation.invoke()
    }
    FenixSnackbar.make(view, FenixSnackbar.LENGTH_LONG)
        .setText(message)
        .setAction(undoActionTitle) {
            undoJob.cancel()
        }.show()
}

internal const val UNDO_DELAY = 3000L
