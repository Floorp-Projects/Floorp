/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.ext

import android.view.inputmethod.EditorInfo
import android.widget.EditText

/**
 * Extension function to handle keyboard Done action
 * @param actionConsumed true if you have consumed the action, else false.
 * @param onDonePressed callback to execute when Done key is pressed
 */
internal fun EditText.onDone(actionConsumed: Boolean, onDonePressed: () -> Unit) {
    setOnEditorActionListener { _, actionId, _ ->
        when (actionId) {
            EditorInfo.IME_ACTION_DONE -> {
                onDonePressed()
                actionConsumed
            }
            else -> false
        }
    }
}
