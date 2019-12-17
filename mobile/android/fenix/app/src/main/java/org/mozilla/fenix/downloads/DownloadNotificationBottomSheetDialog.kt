/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.appcompat.content.res.AppCompatResources
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.android.synthetic.main.download_notification_layout.*
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.feature.downloads.toMegabyteString
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.theme.ThemeManager

class DownloadNotificationBottomSheetDialog(
    context: Context,
    private val download: DownloadState,
    private val didFail: Boolean,
    private val tryAgain: (Long) -> Unit,
    private val onCannotOpenFile: () -> Unit
    // We must pass in the BottomSheetDialog theme for the transparent window background to apply properly
) : BottomSheetDialog(context, R.style.Theme_MaterialComponents_BottomSheetDialog) {
    override fun onCreate(savedInstanceState: Bundle?) {
        setContentView(R.layout.download_notification_layout)

        if (didFail) {
            download_notification_title.text =
                context.getString(R.string.mozac_feature_downloads_failed_notification_text2)

            download_notification_icon.setImageDrawable(AppCompatResources.getDrawable(
                context,
                mozilla.components.feature.downloads.R.drawable.mozac_feature_download_ic_download_failed
            ))

            download_notification_action_button.apply {
                text = context.getString(
                    mozilla.components.feature.downloads.R.string.mozac_feature_downloads_button_try_again
                )
                setOnClickListener {
                    tryAgain(download.id)
                    context.metrics.track(Event.InAppNotificationDownloadTryAgain)
                    dismiss()
                }
            }
        } else {
            val titleText = context.getString(
                R.string.mozac_feature_downloads_completed_notification_text2
            ) + " (${download.contentLength?.toMegabyteString()})"

            download_notification_title.text = titleText

            download_notification_icon.setImageDrawable(AppCompatResources.getDrawable(
                context,
                mozilla.components.feature.downloads.R.drawable.mozac_feature_download_ic_download_complete
            ))

            download_notification_action_button.apply {
                text = context.getString(
                    mozilla.components.feature.downloads.R.string.mozac_feature_downloads_button_open
                )
                setOnClickListener {
                    val fileWasOpened = AbstractFetchDownloadService.openFile(
                        context = context,
                        contentType = download.contentType,
                        filePath = download.filePath
                    )

                    if (!fileWasOpened) {
                        onCannotOpenFile()
                    }

                    context.metrics.track(Event.InAppNotificationDownloadOpen)
                    dismiss()
                }
            }
        }

        download_notification_close_button.setOnClickListener {
            dismiss()
        }

        download_notification_filename.text = download.fileName

        setOnShowListener {
            window?.apply {
                setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
                setLayout(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                )
                navigationBarColor = ContextCompat.getColor(
                        context,
                        ThemeManager.resolveAttribute(R.attr.foundation, context
                    )
                )
            }
        }
    }
}
