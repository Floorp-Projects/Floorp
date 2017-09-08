/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.extensions;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ResourceDrawableUtils;
import org.mozilla.gecko.R;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

public class ExtensionPermissionsHelper implements BundleEventListener {
    private final Context mContext;

    public ExtensionPermissionsHelper(Context context) {
        mContext = context;

        EventDispatcher.getInstance().registerUiThreadListener(this,
            "Extension:PermissionPrompt");
    }

    public void uninit() {
        EventDispatcher.getInstance().unregisterUiThreadListener(this,
            "Extension:PermissionPrompt");
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("Extension:PermissionPrompt".equals(event)) {
            final AlertDialog.Builder builder = new AlertDialog.Builder(mContext);

            final View view = LayoutInflater.from(mContext)
                .inflate(R.layout.extension_permissions_dialog, null);
            builder.setView(view);

            final TextView headerText = (TextView) view.findViewById(R.id.extension_permission_header);
            headerText.setText(message.getString("header"));

            final TextView bodyText = (TextView) view.findViewById(R.id.extension_permission_body);
            bodyText.setText(message.getString("message"));

            builder.setPositiveButton(message.getString("acceptText"), new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int whichButton) {
                    callback.sendSuccess(true);
                }
            });
            builder.setNegativeButton(message.getString("cancelText"), new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int whichButton) {
                    callback.sendSuccess(false);
                }
            });

            final String iconUrl = message.getString("icon");
            if ("DEFAULT".equals(iconUrl)) {
                headerText.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_extension, 0, 0, 0);
            } else {
                ResourceDrawableUtils.getDrawable(mContext, iconUrl, new ResourceDrawableUtils.BitmapLoader() {
                        @Override
                        public void onBitmapFound(final Drawable d) {
                            headerText.setCompoundDrawablesWithIntrinsicBounds(d, null, null, null);
                        }
                    });
            }

            final AlertDialog dialog = builder.create();
            dialog.show();
        }
    }
}
