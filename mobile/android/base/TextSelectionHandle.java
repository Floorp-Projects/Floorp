 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.LayerView;

import org.json.JSONObject;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.PointF;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;

class TextSelectionHandle extends ImageView implements View.OnTouchListener {
    private static final String LOGTAG = "GeckoTextSelectionHandle";

    private enum HandleType { START, MIDDLE, END }; 

    private final HandleType mHandleType;
    private final int mWidth;
    private final int mHeight;
    private final int mShadow;

    private float mLeft;
    private float mTop;
    private boolean mIsRTL; 
    private PointF mGeckoPoint;
    private float mTouchStartX;
    private float mTouchStartY;
    private int mLayerViewX;
    private int mLayerViewY;

    private RelativeLayout.LayoutParams mLayoutParams;

    private static final int IMAGE_LEVEL_LTR = 0;
    private static final int IMAGE_LEVEL_RTL = 1;

    public TextSelectionHandle(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOnTouchListener(this);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TextSelectionHandle);
        int handleType = a.getInt(R.styleable.TextSelectionHandle_handleType, 0x01);

        if (handleType == 0x01)
            mHandleType = HandleType.START;
        else if (handleType == 0x02)
            mHandleType = HandleType.MIDDLE;
        else
            mHandleType = HandleType.END;

        mIsRTL = false;
        mGeckoPoint = new PointF(0.0f, 0.0f);

        mWidth = getResources().getDimensionPixelSize(R.dimen.text_selection_handle_width);
        mHeight = getResources().getDimensionPixelSize(R.dimen.text_selection_handle_height);
        mShadow = getResources().getDimensionPixelSize(R.dimen.text_selection_handle_shadow);
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                mTouchStartX = event.getX();
                mTouchStartY = event.getY();

                int[] rect = new int[2];
                GeckoAppShell.getLayerView().getLocationOnScreen(rect);
                mLayerViewX = rect[0];
                mLayerViewY = rect[1];
                break;
            }
            case MotionEvent.ACTION_UP: {
                mTouchStartX = 0;
                mTouchStartY = 0;

                // Reposition handles to line up with ends of selection
                JSONObject args = new JSONObject();
                try {
                    args.put("handleType", mHandleType.toString());
                } catch (Exception e) {
                    Log.e(LOGTAG, "Error building JSON arguments for TextSelection:Position");
                }
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("TextSelection:Position", args.toString()));
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                move(event.getRawX(), event.getRawY());
                break;
            }
        }
        return true;
    }

    private void move(float newX, float newY) {
        // newX and newY are absolute coordinates, so we need to adjust them to
        // account for other views on the screen (such as the URL bar). We also
        // need to include the offset amount of the touch location relative to
        // the top left of the handle (mTouchStartX and mTouchStartY).
        mLeft = newX - mLayerViewX - mTouchStartX;
        mTop = newY - mLayerViewY - mTouchStartY;

        LayerView layerView = GeckoAppShell.getLayerView();
        if (layerView == null) {
            Log.e(LOGTAG, "Can't move selection because layerView is null");
            return;
        }
        // Send x coordinate on the right side of the start handle, left side of the end handle.
        float left = mLeft + adjustLeftForHandle();

        PointF geckoPoint = new PointF(left, mTop);
        geckoPoint = layerView.convertViewPointToLayerPoint(geckoPoint);

        JSONObject args = new JSONObject();
        try {
            args.put("handleType", mHandleType.toString());
            args.put("x", (int) geckoPoint.x);
            args.put("y", (int) geckoPoint.y);
        } catch (Exception e) {
            Log.e(LOGTAG, "Error building JSON arguments for TextSelection:Move");
        }
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("TextSelection:Move", args.toString()));

        // If we're positioning a cursor, don't move the handle here. Gecko
        // will tell us the position of the caret, so we set the handle
        // position then. This allows us to lock the handle to wherever the
        // caret appears.
        if (!mHandleType.equals(HandleType.MIDDLE)) {
            setLayoutPosition();
        }
    }

    void positionFromGecko(int left, int top, boolean rtl) {
        LayerView layerView = GeckoAppShell.getLayerView();
        if (layerView == null) {
            Log.e(LOGTAG, "Can't position handle because layerView is null");
            return;
        }

        mGeckoPoint = new PointF(left, top);
        if (mIsRTL != rtl) {
            mIsRTL = rtl;
            setImageLevel(mIsRTL ? IMAGE_LEVEL_RTL : IMAGE_LEVEL_LTR);
        }

        ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
        PointF offset = metrics.getMarginOffset();
        repositionWithViewport(metrics.viewportRectLeft - offset.x, metrics.viewportRectTop - offset.y, metrics.zoomFactor);
    }

    void repositionWithViewport(float x, float y, float zoom) {
        PointF viewPoint = new PointF((mGeckoPoint.x * zoom) - x,
                                      (mGeckoPoint.y * zoom) - y);

        mLeft = viewPoint.x - adjustLeftForHandle();
        mTop = viewPoint.y;

        setLayoutPosition();
    }

    private float adjustLeftForHandle() {
        if (mHandleType.equals(HandleType.START))
            return mIsRTL ? mShadow : mWidth - mShadow;
        else if (mHandleType.equals(HandleType.MIDDLE))
            return mWidth / 2;
        else
            return mIsRTL ? mWidth - mShadow : mShadow;
    }

    private void setLayoutPosition() {
        if (mLayoutParams == null) {
            mLayoutParams = (RelativeLayout.LayoutParams) getLayoutParams();
            // Set negative right/bottom margins so that the handles can be dragged outside of
            // the content area (if they are dragged to the left/top, the dyanmic margins set
            // below will take care of that).
            mLayoutParams.rightMargin = 0 - mWidth;
            mLayoutParams.bottomMargin = 0 - mHeight;
        }

        mLayoutParams.leftMargin = (int) mLeft;
        mLayoutParams.topMargin = (int) mTop;
        setLayoutParams(mLayoutParams);
    }
}
