/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.widget.ThemedImageButton;
import org.mozilla.gecko.widget.ThemedLinearLayout;
import org.mozilla.gecko.widget.ThemedTextView;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.Region;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Checkable;
import android.widget.ImageView;

public class TabStripItemView extends ThemedLinearLayout
                              implements Checkable {
    @SuppressWarnings("unused")
    private static final String LOGTAG = "GeckoTabStripItem";

    private static final int[] STATE_CHECKED = {
        android.R.attr.state_checked
    };

    private int id = -1;
    private boolean checked;

    private final ImageView faviconView;
    private final ThemedTextView titleView;
    private final ThemedImageButton closeView;

    private final Paint tabPaint;
    private final Path tabShape;
    private final Region tabRegion;
    private final Region tabClipRegion;

    private final int faviconSize;
    private Bitmap lastFavicon;

    public TabStripItemView(Context context) {
        this(context, null);
    }

    public TabStripItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(HORIZONTAL);

        tabShape = new Path();
        tabRegion = new Region();
        tabClipRegion = new Region();

        tabPaint = new Paint();
        tabPaint.setAntiAlias(true);
        tabPaint.setColor(0xFFFF0000);
        tabPaint.setStrokeWidth(0.0f);
        tabPaint.setXfermode(new PorterDuffXfermode(Mode.DST_IN));

        faviconSize = getResources().getDimensionPixelSize(R.dimen.tab_strip_favicon_size);

        LayoutInflater.from(context).inflate(R.layout.tab_strip_item_view, this);
        setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (id < 0) {
                    throw new IllegalStateException("Invalid tab id:" + id);
                }

                Tabs.getInstance().selectTab(id);
            }
        });

        faviconView = (ImageView) findViewById(R.id.favicon);
        titleView = (ThemedTextView) findViewById(R.id.title);

        closeView = (ThemedImageButton) findViewById(R.id.close);
        closeView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (id < 0) {
                    throw new IllegalStateException("Invalid tab id:" + id);
                }

                final Tabs tabs = Tabs.getInstance();
                tabs.closeTab(tabs.getTab(id), true);
            }
        });
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        tabShape.reset();

        final int curveWidth = TabCurve.getWidthForHeight(height);

        tabShape.moveTo(0, height);
        TabCurve.drawFromBottom(tabShape, 0, height, TabCurve.Direction.RIGHT);
        tabShape.lineTo(width - curveWidth, 0);

        TabCurve.drawFromTop(tabShape, width - curveWidth, height, TabCurve.Direction.RIGHT);
        tabShape.lineTo(0, height);

        tabClipRegion.set(0, 0, width, height);
        tabRegion.setPath(tabShape, tabClipRegion);
    }

    @Override
    public void draw(Canvas canvas) {
        final int saveCount = canvas.saveLayer(0, 0,
                                               getWidth(), getHeight(), null,
                                               Canvas.MATRIX_SAVE_FLAG |
                                               Canvas.CLIP_SAVE_FLAG |
                                               Canvas.HAS_ALPHA_LAYER_SAVE_FLAG |
                                               Canvas.FULL_COLOR_LAYER_SAVE_FLAG |
                                               Canvas.CLIP_TO_LAYER_SAVE_FLAG);

        super.draw(canvas);

        canvas.drawPath(tabShape, tabPaint);

        canvas.restoreToCount(saveCount);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        final int x = (int) event.getX();
        final int y = (int) event.getY();

        // Let motion events through if they're off the tab shape bounds.
        if (action == MotionEvent.ACTION_DOWN && !tabRegion.contains(x, y)) {
            return false;
        }

        return super.onTouchEvent(event);
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (checked) {
            mergeDrawableStates(drawableState, STATE_CHECKED);
        }

        return drawableState;
    }

    @Override
    public boolean isChecked() {
        return checked;
    }

    @Override
    public void setChecked(boolean checked) {
        if (this.checked == checked) {
            return;
        }

        this.checked = checked;
        refreshDrawableState();
    }

    @Override
    public void toggle() {
        setChecked(!checked);
    }

    @Override
    public void setPressed(boolean pressed) {
        super.setPressed(pressed);

        // The surrounding tab strip dividers need to be hidden
        // when a tab item enters pressed state.
        View parent = (View) getParent();
        parent.invalidate();
    }

    void updateFromTab(Tab tab) {
        if (tab == null) {
            return;
        }

        id = tab.getId();
        updateFavicon(tab.getFavicon());
        titleView.setText(tab.getDisplayTitle());
        setPrivateMode(tab.isPrivate());
    }

    private void updateFavicon(final Bitmap favicon) {
        if (favicon == null) {
            lastFavicon = null;
            faviconView.setImageResource(R.drawable.favicon_none);
            return;
        } else if (favicon == lastFavicon) {
            return;
        }

        // Cache the original so we can debounce without scaling.
        lastFavicon = favicon;

        final Bitmap scaledFavicon =
                Bitmap.createScaledBitmap(favicon, faviconSize, faviconSize, false);
        faviconView.setImageBitmap(scaledFavicon);
    }
}
