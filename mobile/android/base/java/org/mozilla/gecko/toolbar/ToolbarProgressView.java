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

import android.support.v4.content.ContextCompat;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.themed.ThemedImageView;
import org.mozilla.gecko.util.WeakReferenceHandler;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Message;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.Animation;

/**
 * Progress view used for page loads.
 *
 * Because we're given limited information about the page load progress, the
 * bar also includes incremental animation between each step to improve
 * perceived performance.
 */
public class ToolbarProgressView extends ThemedImageView {
    private static final int MAX_PROGRESS = 10000;
    private static final int MSG_UPDATE = 0;
    private static final int MSG_HIDE = 1;
    private static final int STEPS = 10;
    private static final int DELAY = 40;

    private int mTargetProgress;
    private int mIncrement;
    private Rect mBounds;
    private Handler mHandler;
    private int mCurrentProgress;

    private PorterDuffColorFilter mPrivateBrowsingColorFilter;

    public ToolbarProgressView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(context);
    }

    public ToolbarProgressView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    private void init(Context ctx) {
        mBounds = new Rect(0,0,0,0);
        mTargetProgress = 0;

        mPrivateBrowsingColorFilter = new PorterDuffColorFilter(
                ContextCompat.getColor(ctx, R.color.private_browsing_purple), PorterDuff.Mode.SRC_IN);

        mHandler = new ToolbarProgressHandler(this);
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

        clearMessages();
    }

    /**
     * Animates the progress bar from the current progress value to the given
     * progress percentage.
     *
     * @param progress Percentage (0-100) to which progress bar should be animated
     */
    void animateProgress(int progressPercentage) {
        final int absoluteProgress = getAbsoluteProgress(progressPercentage);
        if (absoluteProgress <= mTargetProgress) {
            // After we manually click stop, we can still receive page load
            // events (e.g., DOMContentLoaded). Updating for other updates
            // after a STOP event can freeze the progress bar, so guard against
            // that here.
            return;
        }

        mTargetProgress = absoluteProgress;
        mIncrement = (mTargetProgress - mCurrentProgress) / STEPS;

        clearMessages();
        mHandler.sendEmptyMessage(MSG_UPDATE);
    }

    private void clearMessages() {
        mHandler.removeMessages(MSG_UPDATE);
        mHandler.removeMessages(MSG_HIDE);
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

    @Override
    public void setPrivateMode(final boolean isPrivate) {
        super.setPrivateMode(isPrivate);

        // Note: android:tint is better but ColorStateLists are not supported until API 21.
        if (isPrivate) {
            setColorFilter(mPrivateBrowsingColorFilter);
        } else {
            clearColorFilter();
        }
    }

    private static class ToolbarProgressHandler extends WeakReferenceHandler<ToolbarProgressView> {
        public ToolbarProgressHandler(final ToolbarProgressView that) {
            super(that);
        }

        @Override
        public void handleMessage(Message msg) {
            final ToolbarProgressView that = mTarget.get();
            if (that == null) {
                return;
            }

            switch (msg.what) {
                case MSG_UPDATE:
                    that.mCurrentProgress = Math.min(that.mTargetProgress, that.mCurrentProgress + that.mIncrement);

                    that.updateBounds();

                    if (that.mCurrentProgress < that.mTargetProgress) {
                        final int delay = (that.mTargetProgress < MAX_PROGRESS) ? DELAY : DELAY / 4;
                        sendMessageDelayed(that.mHandler.obtainMessage(msg.what), delay);
                    } else if (that.mCurrentProgress == MAX_PROGRESS) {
                        sendMessageDelayed(that.mHandler.obtainMessage(MSG_HIDE), DELAY);
                    }
                    break;

                case MSG_HIDE:
                    that.setVisibility(View.GONE);
                    break;
            }
        }
    };
}
