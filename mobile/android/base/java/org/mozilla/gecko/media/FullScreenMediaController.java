/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.content.Context;

import android.graphics.Color;

import android.view.View;

import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.MediaController;

public class FullScreenMediaController extends MediaController {
    private FullScreenListener mFullScreenListener;

    public FullScreenMediaController(Context ctx) {
        super(ctx);
    }

    public void setFullScreenListener(FullScreenListener listener) {
        mFullScreenListener = listener;
    }

    private void onFullScreenClicked() {
        if (this.mFullScreenListener != null) {
            mFullScreenListener.onFullScreenClicked();
        }
    }

    @Override
    public void setAnchorView(final View view) {
        super.setAnchorView(view);

        // Add the fullscreen button here because this is where the parent class actually creates
        // the media buttons and their layout.
        //
        // http://androidxref.com/6.0.1_r10/xref/frameworks/base/core/java/android/widget/MediaController.java#239
        ImageButton button = new ImageButton(getContext());
        button.setBackgroundColor(Color.TRANSPARENT);
        button.setImageResource(android.R.drawable.picture_frame);
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                FullScreenMediaController.this.onFullScreenClicked();
            }
        });

        // The media buttons are in a horizontal linear layout which is itself packed into
        // a vertical layout. The vertical layout is the only child of the FrameLayout which
        // MediaController inherits from.
        LinearLayout child = (LinearLayout)getChildAt(0);
        LinearLayout buttons = (LinearLayout)child.getChildAt(0);

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT);
        buttons.addView(button, params);
    }

    public interface FullScreenListener {
        void onFullScreenClicked();
    }
}