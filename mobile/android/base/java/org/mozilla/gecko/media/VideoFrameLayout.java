package org.mozilla.gecko.media;

import android.content.Context;

import android.graphics.Color;

import android.util.AttributeSet;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.VideoView;

import org.mozilla.gecko.R;

public class VideoFrameLayout extends FrameLayout {
    private VideoView mVideo;
    private boolean mIsFullScreen;

    public VideoFrameLayout(Context ctx) {
        this(ctx, null);
    }

    public VideoFrameLayout(Context ctx, AttributeSet attrs) {
        this(ctx, attrs, 0);
    }

    public VideoFrameLayout(Context ctx, AttributeSet attrs, int defStyle) {
        super(ctx, attrs, defStyle);
    }

    public void setVideo(VideoView video) {
        if (mVideo != null) {
            removeVideo();
        }

        mVideo = video;

        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.CENTER);

        addView(mVideo, layoutParams);
        setVisibility(View.VISIBLE);

        mVideo.setZOrderOnTop(true);
    }

    public boolean isFullScreen() {
        return mIsFullScreen;
    }

    public void setFullScreen(boolean fullScreen) {
        mIsFullScreen = fullScreen;
        if (fullScreen) {
            setBackgroundColor(Color.BLACK);
        } else {
            setBackgroundResource(R.color.dark_transparent_overlay);
        }
    }

    public boolean hasVideo() {
        return mVideo != null;
    }

    public void removeVideo() {
        removeAllViews();
        setVisibility(View.GONE);
        mVideo = null;
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
}