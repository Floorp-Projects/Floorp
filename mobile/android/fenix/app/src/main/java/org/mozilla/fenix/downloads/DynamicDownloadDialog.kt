/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.content.Context
import android.text.method.ScrollingMovementMethod
import android.view.View
import android.view.ViewGroup
import android.webkit.MimeTypeMap
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.feature.downloads.toMegabyteOrKilobyteString
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.DownloadDialogLayoutBinding
import org.mozilla.fenix.ext.settings

/**
 * [DynamicDownloadDialog] is used to show a view in the current tab to the user, triggered when
 *  downloadFeature.onDownloadStopped gets invoked. It uses [DynamicDownloadDialogBehavior] to
 *  hide when the users scrolls through a website as to not impede his activities.
 */
@Suppress("LongParameterList")
class DynamicDownloadDialog(
    private val context: Context,
    private val downloadState: DownloadState?,
    private val didFail: Boolean,
    private val tryAgain: (String) -> Unit,
    private val onCannotOpenFile: (DownloadState) -> Unit,
    private val binding: DownloadDialogLayoutBinding,
    private val bottomToolbarHeight: Int,
    private val onDismiss: () -> Unit,
) {

    private val settings = context.settings()

    init {
        setupDownloadDialog()
    }

    private fun setupDownloadDialog() {
        if (downloadState == null) return
        binding.root.apply {
            if (layoutParams is CoordinatorLayout.LayoutParams) {
                (layoutParams as CoordinatorLayout.LayoutParams).apply {
                    behavior =
                        DynamicDownloadDialogBehavior<View>(
                            context,
                            null,
                            bottomToolbarHeight.toFloat(),
                        )
                }
            }
        }

        if (settings.shouldUseBottomToolbar) {
            val params: ViewGroup.MarginLayoutParams =
                binding.root.layoutParams as ViewGroup.MarginLayoutParams
            params.bottomMargin = bottomToolbarHeight
        }

        if (didFail) {
            binding.downloadDialogTitle.text =
                context.getString(R.string.mozac_feature_downloads_failed_notification_text2)

            binding.downloadDialogIcon.setImageResource(
                mozilla.components.feature.downloads.R.drawable.mozac_feature_download_ic_download_failed,
            )

            binding.downloadDialogActionButton.apply {
                text = context.getString(
                    mozilla.components.feature.downloads.R.string.mozac_feature_downloads_button_try_again,
                )
                setOnClickListener {
                    tryAgain(downloadState.id)
                    dismiss()
                }
            }
        } else {
            val titleText = context.getString(
                R.string.mozac_feature_downloads_completed_notification_text2,
            ) + " (${downloadState.contentLength?.toMegabyteOrKilobyteString()})"

            binding.downloadDialogTitle.text = titleText

            binding.downloadDialogIcon.setImageResource(
                mozilla.components.feature.downloads.R.drawable.mozac_feature_download_ic_download_complete,
            )

            binding.downloadDialogActionButton.apply {
                text = context.getString(
                    mozilla.components.feature.downloads.R.string.mozac_feature_downloads_button_open,
                )
                setOnClickListener {
                    val fileWasOpened = AbstractFetchDownloadService.openFile(
                        applicationContext = context.applicationContext,
                        download = downloadState,
                    )

                    if (!fileWasOpened) {
                        onCannotOpenFile(downloadState)
                    }

                    dismiss()
                }
            }
        }

        binding.downloadDialogCloseButton.setOnClickListener {
            dismiss()
        }

        binding.downloadDialogFilename.text = downloadState.fileName
        binding.downloadDialogFilename.movementMethod = ScrollingMovementMethod()
    }

    fun show() {
        binding.root.visibility = View.VISIBLE

        (binding.root.layoutParams as CoordinatorLayout.LayoutParams).apply {
            (behavior as DynamicDownloadDialogBehavior).forceExpand(binding.root)
        }
    }

    private fun dismiss() {
        binding.root.visibility = View.GONE
        onDismiss()
    }

    companion object {
        fun getCannotOpenFileErrorMessage(context: Context, download: DownloadState): String {
            val fileExt = MimeTypeMap.getFileExtensionFromUrl(
                download.filePath,
            )
            return context.getString(
                R.string.mozac_feature_downloads_open_not_supported1,
                fileExt,
            )
        }
    }
}
