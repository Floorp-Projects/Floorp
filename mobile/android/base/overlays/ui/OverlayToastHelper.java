/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import org.mozilla.gecko.R;

/**
 * Static helper class for generating toasts for share events.
 *
 * The overlay toasts come in a variety of flavours: success (rectangle with happy green tick,
 * failure (no tick, a retry button), and success-with-tutorial (as success, but with a pretty
 * picture of some description to educate the user on how to use the feature) TODO: Bug 1048645.
 */
public class OverlayToastHelper {

    /**
     * Show a toast indicating a failure to share.
     * @param context Context in which to inflate the toast.
     * @param failureMessage String to display in the toast.
     */
    public static void showFailureToast(Context context, String failureMessage) {
        showToast(context, failureMessage, false);
    }

    /**
     * Show a toast indicating a successful share.
     * @param successMessage Message to show in the toast.
     */
    public static void showSuccessToast(Context context, String successMessage) {
        showToast(context, successMessage, true);
    }

    private static void showToast(Context context, String message, boolean success) {
        LayoutInflater inflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        View layout = inflater.inflate(R.layout.overlay_share_toast, null);

        TextView text = (TextView) layout.findViewById(R.id.overlay_toast_message);
        text.setText(message);

        if (!success) {
            // Hide the happy green tick.
            text.setCompoundDrawables(null, null, null, null);
        }

        Toast toast = new Toast(context);
        toast.setDuration(Toast.LENGTH_SHORT);
        toast.setView(layout);
        toast.show();
    }
}
