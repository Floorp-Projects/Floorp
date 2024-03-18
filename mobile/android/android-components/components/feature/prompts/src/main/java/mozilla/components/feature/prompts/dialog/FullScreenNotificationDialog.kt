/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.app.Dialog
import android.os.Bundle
import android.view.Gravity
import android.view.WindowManager
import androidx.annotation.LayoutRes
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentManager
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private const val TAG = "mozac_feature_prompts_full_screen_notification_dialog"
private const val SNACKBAR_DURATION_LONG_MS = 3000L

/**
 * UI to show a 'full screen mode' notification.
 */
interface FullScreenNotification {
    /**
     * Show the notification.
     *
     * @param fragmentManager the [FragmentManager] to add this notification to.
     */
    fun show(fragmentManager: FragmentManager)
}

/**
 * [DialogFragment] that is configured to match the style and behaviour of a Snackbar.
 *
 * @property layout the layout to use for the dialog.
 */
class FullScreenNotificationDialog(@LayoutRes val layout: Int) :
    DialogFragment(), FullScreenNotification {
    override fun show(fragmentManager: FragmentManager) = super.show(fragmentManager, TAG)

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog = requireActivity().let {
        val view = layoutInflater.inflate(layout, null)
        AlertDialog.Builder(it).setView(view).create()
    }

    override fun onStart() {
        super.onStart()

        dialog?.let { dialog ->
            dialog.window?.let { window ->
                // Prevent any user input from key or other button events to it.
                window.setFlags(
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                )

                window.setGravity(Gravity.BOTTOM)
                window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND)
            }
        }

        // Attempt to automatically dismiss the dialog after the given duration.
        lifecycleScope.launch {
            delay(SNACKBAR_DURATION_LONG_MS)
            dialog?.dismiss()
        }
    }
}
