/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.downloads

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.LinearLayout
import android.widget.RelativeLayout
import androidx.annotation.StringRes
import androidx.annotation.StyleRes
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import androidx.core.view.marginBottom
import androidx.core.view.marginStart
import androidx.core.view.marginTop
import mozilla.components.feature.downloads.databinding.MozacDownloadsPromptBinding

/**
 * A confirmation dialog to be called before a download is triggered.
 * Meant to be used in collaboration with [DownloadsFeature]
 *
 * [SimpleDownloadDialogFragment] is the default dialog used by DownloadsFeature if you don't provide a value.
 * It is composed by a title, a negative and a positive bottoms. When the positive button is clicked
 * the download is triggered.
 *
 */
class SimpleDownloadDialogFragment : DownloadDialogFragment() {

    private val safeArguments get() = requireNotNull(arguments)

    @VisibleForTesting
    internal var testingContext: Context? = null

    internal val dialogGravity: Int get() =
        safeArguments.getInt(KEY_DIALOG_GRAVITY, DEFAULT_VALUE)
    internal val dialogShouldWidthMatchParent: Boolean get() =
        safeArguments.getBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT)

    internal val positiveButtonBackgroundColor get() =
        safeArguments.getInt(KEY_POSITIVE_BUTTON_BACKGROUND_COLOR, DEFAULT_VALUE)
    internal val positiveButtonTextColor get() =
        safeArguments.getInt(KEY_POSITIVE_BUTTON_TEXT_COLOR, DEFAULT_VALUE)
    internal val positiveButtonRadius get() =
        safeArguments.getFloat(KEY_POSITIVE_BUTTON_RADIUS, DEFAULT_VALUE.toFloat())
    internal val fileNameEndMargin get() =
        safeArguments.getInt(KEY_FILE_NAME_END_MARGIN, DEFAULT_VALUE)

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
            R.layout.mozac_downloads_prompt,
            null,
            false,
        )

        val binding = MozacDownloadsPromptBinding.bind(rootView)

        with(requireBundle()) {
            binding.title.text = if (getLong(KEY_CONTENT_LENGTH) <= 0L) {
                getString(R.string.mozac_feature_downloads_dialog_download)
            } else {
                val contentSize = getLong(KEY_CONTENT_LENGTH).toMegabyteOrKilobyteString()
                getString(getInt(KEY_TITLE_TEXT, R.string.mozac_feature_downloads_dialog_title2), contentSize)
            }

            if (positiveButtonBackgroundColor != DEFAULT_VALUE) {
                val backgroundTintList = ContextCompat.getColorStateList(
                    requireContext(),
                    positiveButtonBackgroundColor,
                )
                binding.downloadButton.backgroundTintList = backgroundTintList
            }

            if (positiveButtonTextColor != DEFAULT_VALUE) {
                val color = ContextCompat.getColor(requireContext(), positiveButtonTextColor)
                binding.downloadButton.setTextColor(color)
            }

            if (positiveButtonRadius != DEFAULT_VALUE.toFloat()) {
                val shape = GradientDrawable()
                shape.shape = GradientDrawable.RECTANGLE
                shape.setColor(
                    ContextCompat.getColor(
                        requireContext(),
                        positiveButtonBackgroundColor,
                    ),
                )
                shape.cornerRadius = positiveButtonRadius
                binding.downloadButton.background = shape
            }

            if (fileNameEndMargin != DEFAULT_VALUE) {
                binding.filename.layoutParams = RelativeLayout.LayoutParams(
                    RelativeLayout.LayoutParams.WRAP_CONTENT,
                    RelativeLayout.LayoutParams.WRAP_CONTENT,
                ).apply {
                    marginEnd = fileNameEndMargin
                    marginStart = binding.filename.marginStart
                    topMargin = binding.filename.marginTop
                    bottomMargin = binding.filename.marginBottom
                    addRule(RelativeLayout.BELOW, R.id.title)
                    addRule(RelativeLayout.END_OF, R.id.icon)
                    addRule(RelativeLayout.ALIGN_BASELINE, R.id.icon)
                }
            }

            binding.filename.text = getString(KEY_FILE_NAME, "")
            binding.downloadButton.text = getString(
                getInt(KEY_DOWNLOAD_TEXT, R.string.mozac_feature_downloads_dialog_download),
            )

            binding.closeButton.setOnClickListener {
                onCancelDownload()
                dismiss()
            }

            binding.downloadButton.setOnClickListener {
                onStartDownload()
                dismiss()
            }
        }

        return rootView
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
         * A builder method for creating a [SimpleDownloadDialogFragment]
         */
        fun newInstance(
            @StringRes dialogTitleText: Int = R.string.mozac_feature_downloads_dialog_title2,
            @StringRes downloadButtonText: Int = R.string.mozac_feature_downloads_dialog_download,
            @StyleRes themeResId: Int = 0,
            promptsStyling: DownloadsFeature.PromptsStyling? = null,
        ): SimpleDownloadDialogFragment {
            val fragment = SimpleDownloadDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putInt(KEY_DOWNLOAD_TEXT, downloadButtonText)
                putInt(KEY_THEME_ID, themeResId)
                putInt(KEY_TITLE_TEXT, dialogTitleText)

                promptsStyling?.apply {
                    putInt(KEY_DIALOG_GRAVITY, gravity)
                    putBoolean(KEY_DIALOG_WIDTH_MATCH_PARENT, shouldWidthMatchParent)

                    positiveButtonBackgroundColor?.let {
                        putInt(KEY_POSITIVE_BUTTON_BACKGROUND_COLOR, it)
                    }

                    positiveButtonTextColor?.let {
                        putInt(KEY_POSITIVE_BUTTON_TEXT_COLOR, it)
                    }

                    positiveButtonRadius?.let {
                        putFloat(KEY_POSITIVE_BUTTON_RADIUS, it)
                    }

                    fileNameEndMargin?.let {
                        putInt(KEY_FILE_NAME_END_MARGIN, it)
                    }
                }
            }

            fragment.arguments = arguments

            return fragment
        }

        const val KEY_DOWNLOAD_TEXT = "KEY_DOWNLOAD_TEXT"

        // WARNING: If KEY_CONTENT_LENGTH is <= 0, this will be overriden with the default string "Download"
        const val KEY_TITLE_TEXT = "KEY_TITLE_TEXT"
        const val KEY_THEME_ID = "KEY_THEME_ID"

        private const val KEY_POSITIVE_BUTTON_BACKGROUND_COLOR = "KEY_POSITIVE_BUTTON_BACKGROUND_COLOR"
        private const val KEY_POSITIVE_BUTTON_TEXT_COLOR = "KEY_POSITIVE_BUTTON_TEXT_COLOR"
        private const val KEY_POSITIVE_BUTTON_RADIUS = "KEY_POSITIVE_BUTTON_RADIUS"
        private const val KEY_FILE_NAME_END_MARGIN = "KEY_FILE_NAME_END_MARGIN"
        private const val KEY_DIALOG_GRAVITY = "KEY_DIALOG_GRAVITY"
        private const val KEY_DIALOG_WIDTH_MATCH_PARENT = "KEY_DIALOG_WIDTH_MATCH_PARENT"
        private const val DEFAULT_VALUE = Int.MAX_VALUE
    }

    private fun requireBundle(): Bundle {
        return arguments ?: throw IllegalStateException("Fragment $this arguments is not set.")
    }
}
