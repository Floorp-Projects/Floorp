/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.HardwareUtils;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;

public abstract class ArrowPopup extends PopupWindow {
    /* inner-access */ View mAnchor;
    /* inner-access */ ImageView mArrow;

    /* inner-access */ int mArrowWidth;
    private int mYOffset;

    protected LinearLayout mContent;
    protected boolean mInflated;

    protected final Context mContext;

    public ArrowPopup(Context context) {
        super(context);

        mContext = context;

        final Resources res = context.getResources();
        mArrowWidth = res.getDimensionPixelSize(R.dimen.arrow_popup_arrow_width);
        mYOffset = res.getDimensionPixelSize(R.dimen.arrow_popup_arrow_offset);

        setAnimationStyle(R.style.PopupAnimation);
    }

    protected void init() {
        // Hide the default window background. Passing null prevents the below setOutTouchable()
        // call from working, so use an empty BitmapDrawable instead.
        setBackgroundDrawable(new BitmapDrawable(mContext.getResources()));

        // Allow the popup to be dismissed when touching outside.
        setOutsideTouchable(true);

        final int widthSpec = HardwareUtils.isTablet() ? ViewGroup.LayoutParams.WRAP_CONTENT : ViewGroup.LayoutParams.MATCH_PARENT;
        setWindowLayoutMode(widthSpec, ViewGroup.LayoutParams.WRAP_CONTENT);

        final LayoutInflater inflater = LayoutInflater.from(mContext);
        final ArrowPopupLayout layout = (ArrowPopupLayout) inflater.inflate(R.layout.arrow_popup, null);
        setContentView(layout);

        layout.mListener = new ArrowPopupLayout.OnSizeChangedListener() {
            @Override
            public void onSizeChanged() {
                if (mAnchor == null) {
                    return;
                }

                // Remove padding from the width of the anchor when calculating the arrow offset.
                final int anchorWidth = mAnchor.getWidth() - mAnchor.getPaddingLeft() - mAnchor.getPaddingRight();

                // This is the difference between the edge of the anchor view and the edge of the arrow view.
                // We're making an assumption here that the anchor view is wider than the arrow view.
                final int arrowOffset = (anchorWidth - mArrowWidth) / 2 + mAnchor.getPaddingLeft();

                // Calculate the distance between the left edge of the PopupWindow and the anchor.
                final int[] location = new int[2];
                mAnchor.getLocationOnScreen(location);
                final int anchorX = location[0];
                layout.getLocationOnScreen(location);
                final int popupX = location[0];
                final int leftMargin = anchorX - popupX + arrowOffset;

                // Offset the arrow by that amount to make the arrow align with the anchor. The
                // arrow is already offset by the PopupWindow position, so the total arrow offset
                // will be the compounded offset of the PopupWindow itself and the arrow's offset
                // within that window.
                final RelativeLayout.LayoutParams arrowLayoutParams = (RelativeLayout.LayoutParams) mArrow.getLayoutParams();
                arrowLayoutParams.setMargins(leftMargin, 0, 0, 0);
            }
        };

        mArrow = (ImageView) layout.findViewById(R.id.arrow);
        mContent = (LinearLayout) layout.findViewById(R.id.content);

        mInflated = true;
    }

    /**
     * Sets the anchor for this popup.
     *
     * @param anchor Anchor view for positioning the arrow.
     */
    public void setAnchor(View anchor) {
        mAnchor = anchor;
    }

    /**
     * Shows the popup with the arrow pointing to the center of the anchor view. If the anchor
     * isn't visible, the popup will just be shown at the top of the root view.
     */
    public void show() {
        if (!mInflated) {
            throw new IllegalStateException("ArrowPopup#init() must be called before ArrowPopup#show()");
        }

        final int[] anchorLocation = new int[2];
        if (mAnchor != null) {
            mAnchor.getLocationInWindow(anchorLocation);
        }

        // If the anchor is null or out of the window bounds, just show the popup at the top of the
        // root view, keeping the correct X coordinate.
        if (mAnchor == null || anchorLocation[1] < 0) {
            final View decorView = ((Activity) mContext).getWindow().getDecorView();

            // Bug in Android code causes the window layout parameters to be ignored
            // when using showAtLocation() in Gingerbread phones.
            if (Versions.preHC) {
                setWidth(decorView.getWidth());
                setHeight(decorView.getHeight());
            }

            showAtLocation(decorView, Gravity.NO_GRAVITY, anchorLocation[0] - mArrowWidth, 0);
            return;
        }

        // By default, the left edge of the window is aligned directly underneath the anchor. The
        // arrow needs to be directly under the anchor, and since we want some space between the
        // edge of the popup window and the arrow, we offset the window by -mArrowWidth. This is
        // just an arbitrary value chosen to create some space.
        if (isShowing()) {
            update(mAnchor, -mArrowWidth, -mYOffset, -1, -1);
        } else {
            showAsDropDown(mAnchor, -mArrowWidth, -mYOffset);
        }
    }

    private static class ArrowPopupLayout extends RelativeLayout {
        public interface OnSizeChangedListener {
            public void onSizeChanged();
        }

        /* inner-access */ OnSizeChangedListener mListener;

        public ArrowPopupLayout(Context context, AttributeSet attrs, int defStyle) {
            super(context, attrs, defStyle);
        }

        public ArrowPopupLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public ArrowPopupLayout(Context context) {
            super(context);
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            mListener.onSizeChanged();
        }
    }
}
