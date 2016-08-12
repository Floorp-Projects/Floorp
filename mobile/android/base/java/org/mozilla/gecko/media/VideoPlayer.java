/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.content.Context;

import android.graphics.Color;

import android.net.Uri;

import android.util.AttributeSet;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import android.widget.ImageButton;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.MediaController;
import android.widget.VideoView;

import org.mozilla.gecko.R;

public class VideoPlayer extends FrameLayout {
    private VideoView video;
    private FullScreenMediaController controller;
    private FullScreenListener fullScreenListener;

    private boolean isFullScreen;

    public VideoPlayer(Context ctx) {
        this(ctx, null);
    }

    public VideoPlayer(Context ctx, AttributeSet attrs) {
        this(ctx, attrs, 0);
    }

    public VideoPlayer(Context ctx, AttributeSet attrs, int defStyle) {
        super(ctx, attrs, defStyle);
        setFullScreen(false);
        setVisibility(View.GONE);
    }

    public void start(Uri uri) {
        stop();

        video = new VideoView(getContext());
        controller = new FullScreenMediaController(getContext());
        video.setMediaController(controller);
        controller.setAnchorView(video);

        video.setVideoURI(uri);

        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.CENTER);

        addView(video, layoutParams);
        setVisibility(View.VISIBLE);

        video.setZOrderOnTop(true);
        video.start();
    }

    public boolean isPlaying() {
        return video != null;
    }

    public void stop() {
        if (video == null) {
            return;
        }

        removeAllViews();
        setVisibility(View.GONE);
        video.stopPlayback();

        video = null;
        controller = null;
    }

    public void setFullScreenListener(FullScreenListener listener) {
        fullScreenListener = listener;
    }

    public boolean isFullScreen() {
        return isFullScreen;
    }

    public void setFullScreen(boolean fullScreen) {
        isFullScreen = fullScreen;
        if (fullScreen) {
            setBackgroundColor(Color.BLACK);
        } else {
            setBackgroundResource(R.color.dark_transparent_overlay);
        }

        if (controller != null) {
            controller.setFullScreen(fullScreen);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (event.isSystem()) {
            return super.onKeyDown(keyCode, event);
        }
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (event.isSystem()) {
            return super.onKeyUp(keyCode, event);
        }
        return true;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        return true;
    }

    @Override
    public boolean onTrackballEvent(MotionEvent event) {
        super.onTrackballEvent(event);
        return true;
    }

    public interface FullScreenListener {
        void onFullScreenChanged(boolean fullScreen);
    }

    private class FullScreenMediaController extends MediaController {
        private ImageButton mButton;

        public FullScreenMediaController(Context ctx) {
            super(ctx);

            mButton = new ImageButton(getContext());
            mButton.setScaleType(ImageView.ScaleType.FIT_CENTER);
            mButton.setBackgroundColor(Color.TRANSPARENT);
            mButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    FullScreenMediaController.this.onFullScreenClicked();
                }
            });

            updateFullScreenButton(false);
        }

        public void setFullScreen(boolean fullScreen) {
            updateFullScreenButton(fullScreen);
        }

        private void updateFullScreenButton(boolean fullScreen) {
            mButton.setImageResource(fullScreen ? R.drawable.exit_fullscreen : R.drawable.fullscreen);
        }

        private void onFullScreenClicked() {
            if (VideoPlayer.this.fullScreenListener != null) {
                boolean fullScreen = !VideoPlayer.this.isFullScreen();
                VideoPlayer.this.fullScreenListener.onFullScreenChanged(fullScreen);
            }
        }

        @Override
        public void setAnchorView(final View view) {
            super.setAnchorView(view);

            // Add the fullscreen button here because this is where the parent class actually creates
            // the media buttons and their layout.
            //
            // http://androidxref.com/6.0.1_r10/xref/frameworks/base/core/java/android/widget/MediaController.java#239
            //
            // The media buttons are in a horizontal linear layout which is itself packed into
            // a vertical layout. The vertical layout is the only child of the FrameLayout which
            // MediaController inherits from.
            LinearLayout child = (LinearLayout) getChildAt(0);
            LinearLayout buttons = (LinearLayout) child.getChildAt(0);

            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT,
                                                                             LayoutParams.FILL_PARENT);
            params.gravity = Gravity.CENTER_VERTICAL;

            if (mButton.getParent() != null) {
                ((ViewGroup)mButton.getParent()).removeView(mButton);
            }

            buttons.addView(mButton, params);
        }
    }
}