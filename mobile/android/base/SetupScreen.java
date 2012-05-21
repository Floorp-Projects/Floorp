/* -*- Mode: Java; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Dialog;
import android.content.Context;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.Log;
import android.widget.ImageView;

import org.mozilla.gecko.R;

public class SetupScreen extends Dialog
{
    private static final String LOGTAG = "SetupScreen";
    private AnimationDrawable mProgressSpinner;

    public SetupScreen(Context aContext) {
        super(aContext, android.R.style.Theme_NoTitleBar_Fullscreen);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.setup_screen);
        setCancelable(false);

        setTitle(R.string.splash_settingup);

        ImageView spinnerImage = (ImageView)findViewById(R.id.spinner_image);
        mProgressSpinner = (AnimationDrawable)spinnerImage.getBackground();
        spinnerImage.setImageDrawable(mProgressSpinner);
    }

    @Override
    public void onWindowFocusChanged (boolean hasFocus) {
        mProgressSpinner.start();
    }
}
