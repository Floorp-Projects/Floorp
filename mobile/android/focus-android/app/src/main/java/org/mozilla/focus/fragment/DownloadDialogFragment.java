/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.text.Html;
import android.text.Spanned;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.web.Download;

import mozilla.components.utils.DownloadUtils;

/**
 * Fragment displaying a download dialog
 */
public class DownloadDialogFragment extends DialogFragment {
    public static final String FRAGMENT_TAG = "should-download-prompt-dialog";

    public static DownloadDialogFragment newInstance(Download download) {
        DownloadDialogFragment frag = new DownloadDialogFragment();
        final Bundle args = new Bundle();

        final String fileName = DownloadUtils.guessFileName(
                download.getContentDisposition(),
                download.getUrl(),
                download.getMimeType());

        args.putString("fileName", fileName);
        args.putParcelable("download", download);
        frag.setArguments(args);
        return frag;
    }

    public AlertDialog onCreateDialog(Bundle bundle) {
        final String fileName = getArguments().getString("fileName");
        final Download pendingDownload = getArguments().getParcelable("download");

        final AlertDialog.Builder builder = new AlertDialog.Builder(getContext(), R.style.DialogStyle);
        builder.setCancelable(true);
        builder.setTitle(getString(R.string.download_dialog_title));

        final LayoutInflater inflater = getActivity().getLayoutInflater();
        final View dialogView = inflater.inflate(R.layout.download_dialog, null);
        builder.setView(dialogView);

        final ImageView downloadDialogIcon = (ImageView) dialogView.findViewById(R.id.download_dialog_icon);
        final TextView downloadDialogMessage = (TextView) dialogView.findViewById(R.id.download_dialog_file_name);
        final Button downloadDialogCancelButton = (Button) dialogView.findViewById(R.id.download_dialog_cancel);
        final Button downloadDialogDownloadButton = (Button) dialogView.findViewById(R.id.download_dialog_download);
        final TextView downloadDialogWarningMessage = (TextView) dialogView.findViewById(R.id.download_dialog_warning);

        downloadDialogIcon.setImageResource(R.drawable.ic_download);
        downloadDialogMessage.setText(fileName);
        downloadDialogCancelButton.setText(getString(R.string.download_dialog_action_cancel));
        downloadDialogDownloadButton.setText(getString(R.string.download_dialog_action_download));
        downloadDialogWarningMessage.setText(getSpannedTextFromHtml(R.string.download_dialog_warning, R.string.app_name));

        final AlertDialog alert = builder.create();

        setButtonOnClickListener(downloadDialogCancelButton, pendingDownload, false);
        setButtonOnClickListener(downloadDialogDownloadButton, pendingDownload, true);

        return alert;
    }

    private void setButtonOnClickListener(Button button, final Download pendingDownload, final boolean shouldDownload) {
        button.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        sendDownloadDialogButtonClicked(pendingDownload, shouldDownload);
                        TelemetryWrapper.downloadDialogDownloadEvent(shouldDownload);
                        dismiss();
                    }
                });
    }

    public void sendDownloadDialogButtonClicked(Download download, boolean shouldDownload) {
        final DownloadDialogListener listener = (DownloadDialogListener) getTargetFragment();
        if (listener != null) {
            listener.onFinishDownloadDialog(download, shouldDownload);
        }
        dismiss();
    }

    public Spanned getSpannedTextFromHtml(int text, int replaceString) {
        if (Build.VERSION.SDK_INT >= 24) {
            return (Html.fromHtml(String
                    .format(getText(text)
                            .toString(), getString(replaceString)), Html.FROM_HTML_MODE_LEGACY));
        } else {
            return (Html.fromHtml(String
                    .format(getText(text)
                            .toString(), getString(replaceString))));
        }
    }

    public interface DownloadDialogListener {
        void onFinishDownloadDialog(Download download, boolean shouldDownload);
    }

}
