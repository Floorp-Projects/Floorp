/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.app.Dialog
import android.content.Context
import android.os.Bundle
import androidx.annotation.StringRes
import androidx.annotation.StyleRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog

/**
 * This is the default implementation of the [RedirectDialogFragment].
 *
 * It provides an [AlertDialog] giving the user the choice to allow or deny the opening of a
 * third party app.
 *
 * Intents passed are guaranteed to be openable by a non-browser app.
 */
class SimpleRedirectDialogFragment : RedirectDialogFragment() {

    @VisibleForTesting
    internal var testingContext: Context? = null

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        fun getBuilder(themeID: Int): AlertDialog.Builder {
            val context = testingContext ?: requireContext()
            return if (themeID == 0) AlertDialog.Builder(context) else AlertDialog.Builder(context, themeID)
        }

        return with(requireBundle()) {
            val fileName = getString(KEY_INTENT_URL, "")
            val dialogTitleText = getInt(KEY_TITLE_TEXT, R.string.mozac_feature_applinks_confirm_dialog_title)
            val positiveButtonText = getInt(KEY_POSITIVE_TEXT, R.string.mozac_feature_applinks_confirm_dialog_confirm)
            val negativeButtonText = getInt(KEY_NEGATIVE_TEXT, R.string.mozac_feature_applinks_confirm_dialog_deny)
            val themeResId = getInt(KEY_THEME_ID, 0)
            val cancelable = getBoolean(KEY_CANCELABLE, false)

            getBuilder(themeResId)
                .setTitle(dialogTitleText)
                .setMessage(fileName)
                .setPositiveButton(positiveButtonText) { _, _ ->
                    onConfirmRedirect()
                }
                .setNegativeButton(negativeButtonText) { _, _ ->
                    onCancelRedirect()
                }
                .setCancelable(cancelable)
                .create()
        }
    }

    companion object {
        /**
         * A builder method for creating a [SimpleRedirectDialogFragment]
         */
        fun newInstance(
            @StringRes dialogTitleText: Int = R.string.mozac_feature_applinks_confirm_dialog_title,
            @StringRes positiveButtonText: Int = R.string.mozac_feature_applinks_confirm_dialog_confirm,
            @StringRes negativeButtonText: Int = R.string.mozac_feature_applinks_confirm_dialog_deny,
            @StyleRes themeResId: Int = 0,
            cancelable: Boolean = false,
        ): RedirectDialogFragment {
            val fragment = SimpleRedirectDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putInt(KEY_TITLE_TEXT, dialogTitleText)

                putInt(KEY_POSITIVE_TEXT, positiveButtonText)

                putInt(KEY_NEGATIVE_TEXT, negativeButtonText)

                putInt(KEY_THEME_ID, themeResId)

                putBoolean(KEY_CANCELABLE, cancelable)
            }

            fragment.arguments = arguments
            fragment.isCancelable = false

            return fragment
        }

        const val KEY_POSITIVE_TEXT = "KEY_POSITIVE_TEXT"

        const val KEY_NEGATIVE_TEXT = "KEY_NEGATIVE_TEXT"

        const val KEY_TITLE_TEXT = "KEY_TITLE_TEXT"

        const val KEY_THEME_ID = "KEY_THEME_ID"

        const val KEY_CANCELABLE = "KEY_CANCELABLE"
    }

    private fun requireBundle(): Bundle {
        return arguments ?: throw IllegalStateException("Fragment $this arguments is not set.")
    }
}
