/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.AlertDialog;
import android.content.Context;
import android.content.res.Resources;
import android.preference.DialogPreference;
import android.text.method.ScrollingMovementMethod;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

class FontSizePreference extends DialogPreference {
    private static final String LOGTAG = "FontSizePreference";

    private final Context mContext;
    private TextView mPreviewFontView;
    private Button mIncreaseFontButton;
    private Button mDecreaseFontButton;

    public FontSizePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onPrepareDialogBuilder(AlertDialog.Builder builder) {
        final LayoutInflater inflater =
            (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View dialogView = inflater.inflate(R.layout.font_size_preference, null);
        mPreviewFontView = (TextView) dialogView.findViewById(R.id.preview);
        mPreviewFontView.setMovementMethod(new ScrollingMovementMethod());

        mDecreaseFontButton = (Button) dialogView.findViewById(R.id.decrease_preview_font_button);
        mIncreaseFontButton = (Button) dialogView.findViewById(R.id.increase_preview_font_button);

        builder.setView(dialogView);
    }
}
