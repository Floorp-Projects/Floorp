/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.widget.ResizablePathDrawable;
import org.mozilla.gecko.widget.ResizablePathDrawable.NonScaledPathShape;
import org.mozilla.gecko.widget.themed.ThemedImageButton;
import org.mozilla.gecko.widget.themed.ThemedLinearLayout;
import org.mozilla.gecko.widget.themed.ThemedTextView;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.Region;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Checkable;
import android.widget.ImageView;

public class TabStripItemView extends ThemedLinearLayout
                              implements Checkable {
    private static final String LOGTAG = "GeckoTabStripItem";

    private static final int[] STATE_CHECKED = {
        android.R.attr.state_checked
    };

    private int id = -1;
    private boolean checked;

    private final ImageView faviconView;
    private final ThemedTextView titleView;
    private final ThemedImageButton closeView;

    private final ResizablePathDrawable backgroundDrawable;
    private final Region tabRegion;
    private final Region tabClipRegion;
    private boolean tabRegionNeedsUpdate;

    private final int faviconSize;
    private Bitmap lastFavicon;

    public TabStripItemView(Context context) {
        this(context, null);
    }

    public TabStripItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(HORIZONTAL);

        tabRegion = new Region();
        tabClipRegion = new Region();

        final Resources res = context.getResources();

        final ColorStateList tabColors =
                res.getColorStateList(R.color.tab_strip_item_bg);
        backgroundDrawable = new ResizablePathDrawable(new TabCurveShape(), tabColors);
        setBackgroundDrawable(backgroundDrawable);

        faviconSize = res.getDimensionPixelSize(R.dimen.browser_toolbar_favicon_size);

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

        // Queue a tab region update in the next draw() call. We don't
        // update it immediately here because we need the new path from
        // the background drawable to be updated first.
        tabRegionNeedsUpdate = true;
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
    public void draw(Canvas canvas) {
        super.draw(canvas);

        if (tabRegionNeedsUpdate) {
            final Path path = backgroundDrawable.getPath();
            tabClipRegion.set(0, 0, getWidth(), getHeight());
            tabRegion.setPath(path, tabClipRegion);
            tabRegionNeedsUpdate = false;
        }
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
        if (parent != null) {
            parent.invalidate();
        }
    }

    void updateFromTab(Tab tab) {
        if (tab == null) {
            return;
        }

        id = tab.getId();

        updateTitle(tab);
        updateFavicon(tab.getFavicon());
        setPrivateMode(tab.isPrivate());
    }

    private void updateTitle(Tab tab) {
        final String title;

        // Avoid flickering the about:home URL on every load given how often
        // this page is used in the UI.
        if (AboutPages.isAboutHome(tab.getURL())) {
            titleView.setText(R.string.home_title);
        } else {
            titleView.setText(tab.getDisplayTitle());
        }

        // TODO: Set content description to indicate audio is playing.
        if (tab.isAudioPlaying()) {
            titleView.setCompoundDrawablesWithIntrinsicBounds(R.drawable.tab_audio_playing, 0, 0, 0);
        } else {
            titleView.setCompoundDrawables(null, null, null, null);
        }
    }

    private void updateFavicon(final Bitmap favicon) {
        if (favicon == null) {
            lastFavicon = null;
            faviconView.setImageResource(R.drawable.toolbar_favicon_default);
            return;
        }
        if (favicon == lastFavicon) {
            return;
        }

        // Cache the original so we can debounce without scaling.
        lastFavicon = favicon;

        final Bitmap scaledFavicon =
                Bitmap.createScaledBitmap(favicon, faviconSize, faviconSize, false);
        faviconView.setImageBitmap(scaledFavicon);
    }

    private static class TabCurveShape extends NonScaledPathShape {
        @Override
        protected void onResize(float width, float height) {
            final Path path = getPath();

            path.reset();

            final float curveWidth = TabCurve.getWidthForHeight(height);

            path.moveTo(0, height);
            TabCurve.drawFromBottom(path, 0, height, TabCurve.Direction.RIGHT);
            path.lineTo(width - curveWidth, 0);

            TabCurve.drawFromTop(path, width - curveWidth, height, TabCurve.Direction.RIGHT);
            path.lineTo(0, height);
        }
    }
}
