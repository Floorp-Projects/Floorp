/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ui

import android.app.Dialog
import android.content.DialogInterface
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.os.Parcelable
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.LinearLayout
import androidx.annotation.ColorRes
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.content.ContextCompat
import kotlinx.parcelize.Parcelize
import mozilla.components.feature.downloads.R
import mozilla.components.feature.downloads.databinding.MozacDownloadCancelBinding

/**
 * The dialog warns the user that closing last private tab leads to cancellation of active private
 * downloads.
 */
class DownloadCancelDialogFragment : AppCompatDialogFragment() {

    var onAcceptClicked: ((tabId: String?, source: String?) -> Unit)? = null
    var onDenyClicked: (() -> Unit)? = null

    private val safeArguments get() = requireNotNull(arguments)
    private val downloadCount by lazy { safeArguments.getInt(KEY_DOWNLOAD_COUNT) }
    private val tabId by lazy { safeArguments.getString(KEY_TAB_ID) }
    private val source by lazy { safeArguments.getString(KEY_SOURCE) }
    private val promptStyling by lazy { safeArguments.getParcelable(KEY_STYLE) ?: PromptStyling() }
    private val promptText by lazy { safeArguments.getParcelable(KEY_TEXT) ?: PromptText() }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return Dialog(requireContext()).apply {
            requestWindowFeature(Window.FEATURE_NO_TITLE)
            setCanceledOnTouchOutside(true)

            setContainerView(promptStyling.shouldWidthMatchParent, createContainer())

            window?.apply {
                setGravity(promptStyling.gravity)

                if (promptStyling.shouldWidthMatchParent) {
                    setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
                    // This must be called after addContentView, or it won't fully fill to the edge.
                    setLayout(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                    )
                }
            }
        }
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        onDenyClicked?.invoke()
    }

    @Suppress("NestedBlockDepth")
    private fun Dialog.setContainerView(dialogShouldWidthMatchParent: Boolean, rootView: View) {
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

    @Suppress("InflateParams", "NestedBlockDepth")
    private fun createContainer() = LayoutInflater.from(requireContext()).inflate(
        R.layout.mozac_download_cancel,
        null,
        false,
    ).apply {
        with(MozacDownloadCancelBinding.bind(this)) {
            acceptButton.setOnClickListener {
                onAcceptClicked?.invoke(tabId, source)
                dismiss()
            }

            denyButton.setOnClickListener {
                onDenyClicked?.invoke()
                dismiss()
            }

            with(promptText) {
                title.text = getString(titleText)
                body.text = buildWarningText(downloadCount, bodyText)
                acceptButton.text = getString(acceptText)
                denyButton.text = getString(denyText)
            }

            with(promptStyling) {
                positiveButtonBackgroundColor?.let {
                    val backgroundTintList = ContextCompat.getColorStateList(requireContext(), it)
                    acceptButton.backgroundTintList = backgroundTintList

                    // It appears there is not guaranteed way to get background color of a button,
                    // there are always nullable types, hence the code changing the positiveButtonRadius
                    // executes only if positiveButtonBackgroundColor is provided
                    positiveButtonRadius?.let {
                        val shape = GradientDrawable()
                        shape.shape = GradientDrawable.RECTANGLE
                        shape.setColor(
                            ContextCompat.getColor(
                                requireContext(),
                                positiveButtonBackgroundColor,
                            ),
                        )
                        shape.cornerRadius = positiveButtonRadius
                        acceptButton.background = shape
                    }
                }

                positiveButtonTextColor?.let {
                    val color = ContextCompat.getColor(requireContext(), it)
                    acceptButton.setTextColor(color)
                }
            }
        }
    }

    @VisibleForTesting
    internal fun buildWarningText(downloadCount: Int, @StringRes stringId: Int) = String.format(
        getString(stringId),
        downloadCount,
    )

    companion object {
        private const val KEY_DOWNLOAD_COUNT = "KEY_DOWNLOAD_COUNT"
        private const val KEY_TAB_ID = "KEY_TAB_ID"
        private const val KEY_SOURCE = "KEY_SOURCE"
        private const val KEY_STYLE = "KEY_STYLE"
        private const val KEY_TEXT = "KEY_TEXT"

        /**
         * Returns a new instance of [DownloadCancelDialogFragment].
         * @param downloadCount The number of currently active downloads.
         * @param promptStyling Styling properties for the dialog.
         * @param onPositiveButtonClicked A lambda called when the allow button is clicked.
         * @param onNegativeButtonClicked A lambda called when the deny button is clicked.
         */
        fun newInstance(
            downloadCount: Int,
            tabId: String? = null,
            source: String? = null,
            promptText: PromptText? = null,
            promptStyling: PromptStyling? = null,
            onPositiveButtonClicked: ((tabId: String?, source: String?) -> Unit)? = null,
            onNegativeButtonClicked: (() -> Unit)? = null,
        ): DownloadCancelDialogFragment {
            return DownloadCancelDialogFragment().apply {
                this.arguments = Bundle().apply {
                    putInt(KEY_DOWNLOAD_COUNT, downloadCount)
                    tabId?.let { putString(KEY_TAB_ID, it) }
                    source?.let { putString(KEY_SOURCE, it) }
                    promptText?.let { putParcelable(KEY_TEXT, it) }
                    promptStyling?.let { putParcelable(KEY_STYLE, it) }
                }
                this.onAcceptClicked = onPositiveButtonClicked
                this.onDenyClicked = onNegativeButtonClicked
            }
        }
    }

    /**
     * Styling for the downloads cancellation dialog.
     * Note that for [positiveButtonRadius] to be applied,
     * specifying [positiveButtonBackgroundColor] is necessary.
     */
    @Parcelize
    data class PromptStyling(
        val gravity: Int = Gravity.BOTTOM,
        val shouldWidthMatchParent: Boolean = true,
        @ColorRes
        val positiveButtonBackgroundColor: Int? = null,
        @ColorRes
        val positiveButtonTextColor: Int? = null,
        val positiveButtonRadius: Float? = null,
    ) : Parcelable

    /**
     * The class gives an option to override string resources used by [DownloadCancelDialogFragment].
     */
    @Parcelize
    data class PromptText(
        @StringRes
        val titleText: Int = R.string.mozac_feature_downloads_cancel_active_downloads_warning_content_title,
        @StringRes
        val bodyText: Int = R.string.mozac_feature_downloads_cancel_active_private_downloads_warning_content_body,
        @StringRes
        val acceptText: Int = R.string.mozac_feature_downloads_cancel_active_downloads_accept,
        @StringRes
        val denyText: Int = R.string.mozac_feature_downloads_cancel_active_private_downloads_deny,
    ) : Parcelable
}
