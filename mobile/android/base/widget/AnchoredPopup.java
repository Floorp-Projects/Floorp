/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import org.mozilla.gecko.util.HardwareUtils;

/**
 * AnchoredPopup is the base class for doorhanger notifications, and is anchored to the urlbar.
 */
public abstract class AnchoredPopup extends PopupWindow {
    private View mAnchor;

    protected LinearLayout mContent;
    protected boolean mInflated;

    protected final Context mContext;

    public AnchoredPopup(Context context) {
        super(context);

        mContext = context;

        setAnimationStyle(R.style.PopupAnimation);
    }

    protected void init() {
        // Hide the default window background. Passing null prevents the below setOutTouchable()
        // call from working, so use an empty BitmapDrawable instead.
        setBackgroundDrawable(new BitmapDrawable(mContext.getResources()));

        // Allow the popup to be dismissed when touching outside.
        setOutsideTouchable(true);

        // PopupWindow has a default width and height of 0, so set the width here.
        int width = (int) mContext.getResources().getDimension(R.dimen.doorhanger_width);
        setWindowLayoutMode(0, ViewGroup.LayoutParams.WRAP_CONTENT);
        setWidth(width);

        final LayoutInflater inflater = LayoutInflater.from(mContext);
        final View layout = inflater.inflate(R.layout.anchored_popup, null);
        setContentView(layout);

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

        // The doorhanger should overlap the bottom of the urlbar.
        int offsetY = mContext.getResources().getDimensionPixelOffset(R.dimen.doorhanger_offsetY);
        final View decorView = ((Activity) mContext).getWindow().getDecorView();

        // Hack for Gingerbread: showAtLocation ignores window layout parameters so we have to use
        // showAsDropDown() instead.
        // Height and width are always set to 0 dp.
        if (Versions.preHC) {
            setWidth(decorView.getWidth());
            offsetY = mContext.getResources().getDimensionPixelOffset(R.dimen.doorhanger_GB_offsetY);
            if (mAnchor == null) {
              mAnchor = decorView;
            }
            showAsDropDown(mAnchor, 0, -offsetY);
            return;
        }

        // If the anchor is null or out of the window bounds, just show the popup at the top of the
        // root view.
        if (mAnchor == null || anchorLocation[1] < 0) {
            showAtLocation(decorView, Gravity.NO_GRAVITY, 0, offsetY);
            return;
        }

        showAtLocation(mAnchor, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, offsetY);
    }
}
