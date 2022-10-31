/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.downloads.ui

import android.annotation.SuppressLint
import android.app.Dialog
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.widget.AppCompatImageButton
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.downloads.R
import java.util.ArrayList

/**
 * A dialog where an user can select with which app a download must be performed.
 */
internal class DownloadAppChooserDialog : AppCompatDialogFragment() {
    private val safeArguments get() = requireNotNull(arguments)
    internal val appsList: ArrayList<DownloaderApp>
        get() =
            safeArguments.getParcelableArrayList<DownloaderApp>(KEY_APP_LIST) ?: arrayListOf()

    internal val dialogGravity: Int get() =
        safeArguments.getInt(KEY_DIALOG_GRAVITY, DEFAULT_VALUE)
    internal val dialogShouldWidthMatchParent: Boolean get() =
        safeArguments.getBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT)

    /**
     * Indicates the user has selected an application to perform the download
     */
    internal var onAppSelected: ((DownloaderApp) -> Unit) = {}

    internal var onDismiss: () -> Unit = {}

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val sheetDialog = Dialog(requireContext())
        sheetDialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        sheetDialog.setCanceledOnTouchOutside(false)

        val rootView = createContainer()
        sheetDialog.setContainerView(rootView)
        sheetDialog.window?.apply {
            if (dialogGravity != DEFAULT_VALUE) {
                setGravity(dialogGravity)
            }

            if (dialogShouldWidthMatchParent) {
                setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
                // This must be called after addContentView, or it won't fully fill to the edge.
                setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            }
        }
        return sheetDialog
    }

    @SuppressLint("InflateParams")
    private fun createContainer(): View {
        val rootView = LayoutInflater.from(requireContext()).inflate(
            R.layout.mozac_downloader_chooser_prompt,
            null,
            false,
        )

        val recyclerView = rootView.findViewById<RecyclerView>(R.id.apps_list)
        recyclerView.adapter = DownloaderAppAdapter(rootView.context, appsList) { app ->
            onAppSelected(app)
            dismiss()
        }

        rootView.findViewById<AppCompatImageButton>(R.id.close_button).setOnClickListener {
            dismiss()
            onDismiss()
        }

        return rootView
    }

    fun setApps(apps: List<DownloaderApp>) {
        val args = arguments ?: Bundle()
        args.putParcelableArrayList(KEY_APP_LIST, ArrayList(apps))
        arguments = args
    }

    private fun Dialog.setContainerView(rootView: View) {
        if (dialogShouldWidthMatchParent) {
            setContentView(rootView)
        } else {
            addContentView(
                rootView,
                LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.MATCH_PARENT,
                ),
            )
        }
    }

    companion object {
        /**
         * A builder method for creating a [DownloadAppChooserDialog]
         */
        fun newInstance(
            gravity: Int? = DEFAULT_VALUE,
            dialogShouldWidthMatchParent: Boolean? = false,
        ): DownloadAppChooserDialog {
            val fragment = DownloadAppChooserDialog()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                gravity?.let { putInt(KEY_DIALOG_GRAVITY, it) }
                dialogShouldWidthMatchParent?.let { putBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT, it) }
            }

            fragment.arguments = arguments

            return fragment
        }

        private const val KEY_DIALOG_GRAVITY = "KEY_DIALOG_GRAVITY"
        private const val KEY_DIALOG_WIDTH_MATCH_PARENT = "KEY_DIALOG_WIDTH_MATCH_PARENT"
        private const val DEFAULT_VALUE = Int.MAX_VALUE

        private const val KEY_APP_LIST = "KEY_APP_LIST"
        internal const val FRAGMENT_TAG = "SHOULD_APP_DOWNLOAD_PROMPT_DIALOG"
    }
}
