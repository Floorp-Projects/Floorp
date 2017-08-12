/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.widget.themed.ThemedImageButton;
import org.mozilla.gecko.widget.themed.ThemedLinearLayout;
import org.mozilla.gecko.widget.themed.ThemedTextView;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.support.v4.widget.TextViewCompat;
import android.util.AttributeSet;
import android.view.LayoutInflater;
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

    private final int faviconSize;
    private Bitmap lastFavicon;

    public TabStripItemView(Context context) {
        this(context, null);
    }

    public TabStripItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(HORIZONTAL);

        final Resources res = context.getResources();

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

    @RobocopTarget
    public int getTabId() {
        return id;
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

        setChecked(Tabs.getInstance().isSelectedTab(tab));
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
            TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(titleView, R.drawable.tab_audio_playing, 0, 0, 0);
        } else {
            TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(titleView, null, null, null, null);
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
}
