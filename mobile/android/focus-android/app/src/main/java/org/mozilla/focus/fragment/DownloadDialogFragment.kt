/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.os.Build
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.appcompat.app.AlertDialog
import android.text.Html
import android.text.Spanned
import android.widget.Button
import kotlinx.android.synthetic.main.download_dialog.view.*
import mozilla.components.support.utils.DownloadUtils
import org.mozilla.focus.R
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.web.Download

/**
 * Fragment displaying a download dialog
 */
class DownloadDialogFragment : DialogFragment() {

    override fun onCreateDialog(bundle: Bundle?): AlertDialog {
        val fileName = arguments!!.getString("fileName")
        val pendingDownload = arguments!!.getParcelable<Download>("download")

        val builder = AlertDialog.Builder(requireContext(), R.style.DialogStyle)
        builder.setCancelable(true)
        builder.setTitle(getString(R.string.download_dialog_title))

        val inflater = activity!!.layoutInflater
        val dialogView = inflater.inflate(R.layout.download_dialog, null)
        builder.setView(dialogView)

        dialogView.download_dialog_icon.setImageResource(R.drawable.ic_download)
        dialogView.download_dialog_file_name.text = fileName
        dialogView.download_dialog_cancel.text = getString(R.string.download_dialog_action_cancel)
        dialogView.download_dialog_download.text =
                getString(R.string.download_dialog_action_download)
        dialogView.download_dialog_warning.text =
                getSpannedTextFromHtml(R.string.download_dialog_warning, R.string.app_name)

        setButtonOnClickListener(dialogView.download_dialog_cancel, pendingDownload, false)
        setButtonOnClickListener(dialogView.download_dialog_download, pendingDownload, true)

        return builder.create()
    }

    private fun setButtonOnClickListener(
        button: Button,
        pendingDownload: Download?,
        shouldDownload: Boolean
    ) {
        button.setOnClickListener {
            sendDownloadDialogButtonClicked(pendingDownload, shouldDownload)
            TelemetryWrapper.downloadDialogDownloadEvent(shouldDownload)
            dismiss()
        }
    }

    private fun sendDownloadDialogButtonClicked(download: Download?, shouldDownload: Boolean) {
        val listener = parentFragment as DownloadDialogListener?
        listener?.onFinishDownloadDialog(download, shouldDownload)
        dismiss()
    }

    @Suppress("DEPRECATION")
    private fun getSpannedTextFromHtml(text: Int, replaceString: Int): Spanned {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Html.fromHtml(
                String
                    .format(
                        getText(text)
                            .toString(), getString(replaceString)
                    ), Html.FROM_HTML_MODE_LEGACY
            )
        } else {
            Html.fromHtml(
                String
                    .format(
                        getText(text)
                            .toString(), getString(replaceString)
                    )
            )
        }
    }

    interface DownloadDialogListener {
        fun onFinishDownloadDialog(download: Download?, shouldDownload: Boolean)
    }

    companion object {
        val FRAGMENT_TAG = "should-download-prompt-dialog"

        fun newInstance(download: Download): DownloadDialogFragment {
            val frag = DownloadDialogFragment()
            val args = Bundle()

            val fileName = download.fileName ?: DownloadUtils.guessFileName(
                download.contentDisposition,
                download.url,
                download.mimeType
            )

            args.putString("fileName", fileName)
            args.putParcelable("download", download)
            frag.arguments = args
            return frag
        }
    }
}
