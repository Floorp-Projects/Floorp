/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Message;
import android.util.AttributeSet;
import android.util.Log;
import android.widget.ImageView;
import android.view.View;

/**
 * Progress view used for page loads.
 *
 * Because we're given limited information about the page load progress, the
 * bar also includes incremental animation between each step to improve
 * perceived performance.
 */
public class ToolbarProgressView extends ImageView {
    public static final int MAX_PROGRESS = 10000;
    private static final int MSG_UPDATE = 42;
    private static final int STEPS = 10;
    private static final int DELAY = 40;

    private int mTargetProgress;
    private int mIncrement;
    private Rect mBounds;
    private Handler mHandler;
    private int mCurrentProgress;

    public ToolbarProgressView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(context);
    }

    public ToolbarProgressView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public ToolbarProgressView(Context context) {
        super(context);
        init(context);
    }

    private void init(Context ctx) {
        mBounds = new Rect(0,0,0,0);
        mTargetProgress = 0;

        mHandler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                if (msg.what == MSG_UPDATE) {
                    final int progress = Math.min(mTargetProgress, mCurrentProgress + mIncrement);
                    mCurrentProgress = progress;

                    updateBounds();

                    if (progress < mTargetProgress) {
                        sendMessageDelayed(mHandler.obtainMessage(MSG_UPDATE), DELAY);
                    }
                }
            }

        };
    }

    @Override
    public void onLayout(boolean f, int l, int t, int r, int b) {
        mBounds.left = 0;
        mBounds.right = (r - l) * mCurrentProgress / MAX_PROGRESS;
        mBounds.top = 0;
        mBounds.bottom = b - t;
    }

    @Override
    public void onDraw(Canvas canvas) {
        final Drawable d = getDrawable();
        d.setBounds(mBounds);
        d.draw(canvas);
    }

    /**
     * Immediately sets the progress bar to the given progress percentage.
     *
     * @param progress Percentage (0-100) to which progress bar should be set
     */
    void setProgress(int progressPercentage) {
        mCurrentProgress = mTargetProgress = getAbsoluteProgress(progressPercentage);
        updateBounds();

        mHandler.removeMessages(MSG_UPDATE);
    }

    /**
     * Animates the progress bar from the current progress value to the given
     * progress percentage.
     *
     * @param progress Percentage (0-100) to which progress bar should be animated
     */
    void animateProgress(int progressPercentage) {
        final int absoluteProgress = getAbsoluteProgress(progressPercentage);
        if (absoluteProgress == mTargetProgress) {
            return;
        }

        mCurrentProgress = mTargetProgress;
        mTargetProgress = absoluteProgress;
        mIncrement = (mTargetProgress - mCurrentProgress) / STEPS;

        mHandler.removeMessages(MSG_UPDATE);
        mHandler.sendEmptyMessage(MSG_UPDATE);
    }

    private int getAbsoluteProgress(int progressPercentage) {
        if (progressPercentage < 0) {
            return 0;
        }

        if (progressPercentage > 100) {
            return 100;
        }

        return progressPercentage * MAX_PROGRESS / 100;
    }

    private void updateBounds() {
        mBounds.right = getWidth() * mCurrentProgress / MAX_PROGRESS;
        invalidate();
    }
}
