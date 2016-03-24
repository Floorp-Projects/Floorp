 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.LayerView;

import org.json.JSONObject;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Point;
import android.graphics.PointF;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;

/**
 * Text selection handles enable a user to change position of selected text in
 * Gecko's DOM structure.
 *
 * A text "Selection" or nsISelection object, has start and end positions,
 * referred to as Anchor and Focus objects.
 *
 * If the Anchor and Focus objects are at the same point, it represents a text
 * selection Caret, commonly diplayed as a blinking, vertical |.
 *
 * Anchor and Focus objects each represent a DOM node, and character offset
 * from the start of the node. The Anchor always refers to the start of the
 * Selection, and the Focus refers to its end.
 *
 * In LTR languages such as English, the Anchor is to the left of the Focus.
 * In RTL languages such as Hebrew, the Anchor is to the right of the Focus.
 *
 * For multi-line Selections, in both LTR and RTL languages, the Anchor starts
 * above the Focus.
 */
class TextSelectionHandle extends ImageView implements View.OnTouchListener {
    private static final String LOGTAG = "GeckoTextSelectionHandle";

    public enum HandleType { ANCHOR, CARET, FOCUS };

    private final HandleType mHandleType;
    private final int mWidth;
    private final int mHeight;
    private final int mShadow;

    private float mLeft;
    private float mTop;
    private boolean mIsRTL; 
    private PointF mGeckoPoint;
    private PointF mTouchStart;

    private RelativeLayout.LayoutParams mLayoutParams;

    private static final int IMAGE_LEVEL_LTR = 0;
    private static final int IMAGE_LEVEL_RTL = 1;

    public TextSelectionHandle(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOnTouchListener(this);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TextSelectionHandle);
        int handleType = a.getInt(R.styleable.TextSelectionHandle_handleType, 0x01);
        a.recycle();

        if (handleType == 0x01)
            mHandleType = HandleType.ANCHOR;
        else if (handleType == 0x02)
            mHandleType = HandleType.CARET;
        else
            mHandleType = HandleType.FOCUS;

        mGeckoPoint = new PointF(0.0f, 0.0f);
        mTouchStart = new PointF(0.0f, 0.0f);

        mWidth = getResources().getDimensionPixelSize(R.dimen.text_selection_handle_width);
        mHeight = getResources().getDimensionPixelSize(R.dimen.text_selection_handle_height);
        mShadow = getResources().getDimensionPixelSize(R.dimen.text_selection_handle_shadow);
    }

    private int getStatusBarHeight() {
        int result = 0;
        int resourceId = getResources().getIdentifier("status_bar_height", "dimen", "android");
        if (resourceId > 0) {
            result = getResources().getDimensionPixelSize(resourceId);
        }
        return result;
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                mTouchStart.x = event.getX();
                mTouchStart.y = event.getY();
                break;
            }
            case MotionEvent.ACTION_UP: {
                mTouchStart.x = 0;
                mTouchStart.y = 0;

                // Reposition handles to line up with ends of selection
                JSONObject args = new JSONObject();
                try {
                    args.put("handleType", mHandleType.toString());
                } catch (Exception e) {
                    Log.e(LOGTAG, "Error building JSON arguments for TextSelection:Position");
                }
                GeckoAppShell.notifyObservers("TextSelection:Position", args.toString());
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
        LayerView layerView = GeckoAppShell.getLayerView();

        // newX and newY are in screen coordinates, but mLeft/mTop are relative
        // to the ancestor (which is what LayerView is relative to also). So,
        // we need to adjust newX/newY. The |ancestorOrigin| variable computed
        // below is the origin of the ancestor relative to the screen coordinates,
        // so subtracting that from newY puts newY into the desired coordinate
        // space. We also need to include the offset amount of the touch location
        // relative to the top left of the handle (mTouchStart).
        int[] layerViewPosition = new int[2];
        layerView.getLocationOnScreen(layerViewPosition);
        float ancestorOrigin = layerViewPosition[1];

        mLeft = newX - mTouchStart.x;
        mTop = newY - mTouchStart.y - ancestorOrigin;

        // Send x coordinate on the right side of the start handle, left side of the end handle.
        float layerViewTranslation = layerView.getSurfaceTranslation();
        PointF geckoPoint = new PointF(mLeft + adjustLeftForHandle(),
                                       mTop - layerViewTranslation);
        geckoPoint = layerView.convertViewPointToLayerPoint(geckoPoint);

        JSONObject args = new JSONObject();
        try {
            args.put("handleType", mHandleType.toString());
            args.put("x", (int) geckoPoint.x);
            args.put("y", (int) geckoPoint.y);
        } catch (Exception e) {
            Log.e(LOGTAG, "Error building JSON arguments for TextSelection:Move");
        }
        GeckoAppShell.notifyObservers("TextSelection:Move", args.toString());

        // If we're positioning a cursor, don't move the handle here. Gecko
        // will tell us the position of the caret, so we set the handle
        // position then. This allows us to lock the handle to wherever the
        // caret appears.
        if (mHandleType != HandleType.CARET) {
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
        repositionWithViewport(metrics.viewportRectLeft, metrics.viewportRectTop, metrics.zoomFactor);
    }

    void repositionWithViewport(float x, float y, float zoom) {
        PointF viewPoint = new PointF((mGeckoPoint.x * zoom) - x,
                                      (mGeckoPoint.y * zoom) - y);
        mLeft = viewPoint.x - adjustLeftForHandle();
        mTop = viewPoint.y + GeckoAppShell.getLayerView().getSurfaceTranslation();

        setLayoutPosition();
    }

    private float adjustLeftForHandle() {
        if (mHandleType == HandleType.ANCHOR) {
            return mIsRTL ? mShadow : mWidth - mShadow;
        } else if (mHandleType == HandleType.CARET) {
            return mWidth / 2;
        } else {
            return mIsRTL ? mWidth - mShadow : mShadow;
        }
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
