/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;

public class ArrowPopup extends PopupWindow {
    protected final GeckoApp mActivity;

    private View mAnchor;
    private ImageView mArrow;

    private int mArrowWidth;
    private int mYOffset;

    protected LinearLayout mContent;
    protected boolean mInflated;

    public ArrowPopup(GeckoApp aActivity) {
        this(aActivity, null);
    }

    public ArrowPopup(GeckoApp activity, View anchor) {
        super(activity);
        mActivity = activity;
        mAnchor = anchor;

        mInflated = false;

        final Resources res = activity.getResources();
        mArrowWidth = res.getDimensionPixelSize(R.dimen.menu_popup_arrow_width);
        mYOffset = res.getDimensionPixelSize(R.dimen.menu_popup_arrow_offset);

        setAnimationStyle(R.style.PopupAnimation);
    }

    public void setAnchor(View anchor) {
        mAnchor = anchor;
    }

    protected void init() {
        setBackgroundDrawable(new BitmapDrawable());
        setOutsideTouchable(true);

        setWindowLayoutMode(HardwareUtils.isTablet() ? ViewGroup.LayoutParams.WRAP_CONTENT : ViewGroup.LayoutParams.FILL_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        RelativeLayout layout = (RelativeLayout) inflater.inflate(R.layout.arrow_popup, null);
        setContentView(layout);

        mArrow = (ImageView) layout.findViewById(R.id.arrow);
        mContent = (LinearLayout) layout.findViewById(R.id.content);

        mInflated = true;
    }

    /*
     * Shows the popup with the arrow pointing to the center of the anchor view. If an anchor hasn't
     * been set or isn't visible, the popup will just be shown at the top of the gecko app view.
     */
    public void show() {
        int[] anchorLocation = new int[2];
        if (mAnchor != null)
            mAnchor.getLocationInWindow(anchorLocation);

        // If there's no anchor or the anchor is out of the window bounds,
        // just show the popup at the top of the gecko app view.
        if (mAnchor == null || anchorLocation[1] < 0) {
            final View view = mActivity.getView();

            // Bug in android code causes the window layout parameters to be ignored
            // when using showAtLocation() in Gingerbread phones.
            if (Build.VERSION.SDK_INT < 11) {
                setWidth(view.getWidth());
                setHeight(view.getHeight());
            }

            showAtLocation(view, Gravity.TOP, 0, 0);
            return;
        }

        // Remove padding from the width of the anchor when calculating the arrow offset.
        int anchorWidth = mAnchor.getWidth() - mAnchor.getPaddingLeft() - mAnchor.getPaddingRight();
        // This is the difference between the edge of the anchor view and the edge of the arrow view.
        // We're making an assumption here that the anchor view is wider than the arrow view.
        int arrowOffset = (anchorWidth - mArrowWidth)/2 + mAnchor.getPaddingLeft();

        // The horizontal offset of the popup window, relative to the left side of the anchor view.
        int offset = 0;

        RelativeLayout.LayoutParams arrowLayoutParams = (RelativeLayout.LayoutParams) mArrow.getLayoutParams();

        if (HardwareUtils.isTablet()) {
            // On tablets, the popup has a fixed width, so we use a horizontal offset to position it.
            // The arrow's left margin is set by the arrow_popup.xml layout file.
            // This assumes that anchor is not too close to the right side of the screen.
            offset = arrowOffset - arrowLayoutParams.leftMargin;
        } else {
            // On phones, the popup takes up the width of the screen, so we set the arrow's left
            // margin to make it line up with the anchor.
            int leftMargin = anchorLocation[0] + arrowOffset;
            arrowLayoutParams.setMargins(leftMargin, 0, 0, 0);
        }

        if (isShowing()) {
            update(mAnchor, offset, -mYOffset, -1, -1);
        } else {
            showAsDropDown(mAnchor, offset, -mYOffset);
        }
    }
}
