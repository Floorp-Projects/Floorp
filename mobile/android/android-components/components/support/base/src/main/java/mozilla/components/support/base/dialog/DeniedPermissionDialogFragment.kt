/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.dialog

import android.app.Dialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import mozilla.components.support.base.R

internal const val KEY_MESSAGE = "KEY_MESSAGE"

/**
 * A dialog to be displayed when the Android permission is denied,
 * users should be notified and offered a way activate it on the app settings.
 * The dialog will have two buttons: One "Go to settings" and another for "Dismissing".
 */
class DeniedPermissionDialogFragment : DialogFragment() {
    internal val message: Int by lazy { safeArguments.getInt(KEY_MESSAGE) }
    val safeArguments get() = requireNotNull(arguments)

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = AlertDialog.Builder(requireContext())
            .setMessage(message)
            .setCancelable(true)
            .setNegativeButton(R.string.mozac_support_base_permissions_needed_negative_button) { _, _ ->
                dismiss()
            }
            .setPositiveButton(R.string.mozac_support_base_permissions_needed_positive_button) { _, _ ->
                openSettingsPage()
            }
        return builder.create()
    }

    @VisibleForTesting
    internal fun openSettingsPage() {
        dismiss()
        val intent = Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS)
        val uri = Uri.fromParts("package", requireContext().packageName, null)
        intent.data = uri
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        requireContext().startActivity(intent)
    }

    companion object {
        /**
         * A builder method for creating a [DeniedPermissionDialogFragment]
         * @param message the message of the dialog.
         **/
        fun newInstance(
            @StringRes message: Int,
        ): DeniedPermissionDialogFragment {
            val fragment = DeniedPermissionDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putInt(KEY_MESSAGE, message)
            }

            fragment.arguments = arguments
            return fragment
        }

        const val FRAGMENT_TAG = "DENIED_DOWNLOAD_PERMISSION_PROMPT_DIALOG"
    }
}
