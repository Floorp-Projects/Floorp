/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.downloads

import android.app.Dialog
import android.content.Context
import android.os.Bundle
import androidx.annotation.StringRes
import androidx.annotation.StyleRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import mozilla.components.feature.downloads.R.string.mozac_feature_downloads_dialog_cancel
import mozilla.components.feature.downloads.R.string.mozac_feature_downloads_dialog_download
import mozilla.components.feature.downloads.R.string.mozac_feature_downloads_dialog_title

/**
 * A confirmation dialog to be called before a download is triggered.
 * Meant to be used in collaboration with [DownloadsFeature]
 *
 * [SimpleDownloadDialogFragment] is the default dialog use by DownloadsFeature if you don't provide a value.
 * It is composed by a title, a negative and a positive bottoms. When the positive button is clicked
 * the download it triggered.
 *
 */
class SimpleDownloadDialogFragment : DownloadDialogFragment() {

    @VisibleForTesting
    internal var testingContext: Context? = null

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        fun getBuilder(themeID: Int): AlertDialog.Builder {
            val context = testingContext ?: requireContext()
            return if (themeID == 0) AlertDialog.Builder(context) else AlertDialog.Builder(context, themeID)
        }

        return with(requireBundle()) {
            val fileName = getString(KEY_FILE_NAME, "")
            val dialogTitleText = getInt(KEY_TITLE_TEXT, mozac_feature_downloads_dialog_title)
            val positiveButtonText = getInt(KEY_POSITIVE_TEXT, mozac_feature_downloads_dialog_download)
            val negativeButtonText = getInt(KEY_NEGATIVE_TEXT, mozac_feature_downloads_dialog_cancel)
            val themeResId = getInt(KEY_THEME_ID, 0)
            val cancelable = getBoolean(KEY_CANCELABLE, false)

            getBuilder(themeResId)
                .setTitle(dialogTitleText)
                .setMessage(fileName)
                .setPositiveButton(positiveButtonText) { _, _ ->
                    onStartDownload()
                }
                .setNegativeButton(negativeButtonText) { _, _ ->
                    onCancelDownload()
                    dismiss()
                }
                .setOnCancelListener { onCancelDownload() }
                .setCancelable(cancelable)
                .create()
        }
    }

    companion object {
        /**
         * A builder method for creating a [SimpleDownloadDialogFragment]
         */
        fun newInstance(
            @StringRes dialogTitleText: Int = mozac_feature_downloads_dialog_download,
            @StringRes positiveButtonText: Int = mozac_feature_downloads_dialog_download,
            @StringRes negativeButtonText: Int = mozac_feature_downloads_dialog_cancel,
            @StyleRes themeResId: Int = 0,
            cancelable: Boolean = false
        ): SimpleDownloadDialogFragment {
            val fragment = SimpleDownloadDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putInt(KEY_TITLE_TEXT, dialogTitleText)

                putInt(KEY_POSITIVE_TEXT, positiveButtonText)

                putInt(KEY_NEGATIVE_TEXT, negativeButtonText)

                putInt(KEY_THEME_ID, themeResId)

                putBoolean(KEY_CANCELABLE, cancelable)
            }

            fragment.arguments = arguments

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
