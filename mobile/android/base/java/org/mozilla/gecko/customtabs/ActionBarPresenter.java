/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Handler;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.app.ActionBar;
import android.text.TextUtils;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.toolbar.SecurityModeUtil;
import org.mozilla.gecko.toolbar.SiteIdentityPopup;
import org.mozilla.gecko.util.ColorUtil;

/**
 * This class is used to maintain appearance of ActionBar of CustomTabsActivity, includes background
 * color, custom-view and so on.
 */
public class ActionBarPresenter {

    @ColorInt
    private static final int DEFAULT_TEXT_PRIMARY_COLOR = 0xFFFFFFFF;
    private static final long CUSTOM_VIEW_UPDATE_DELAY = 1000;

    private final ActionBar mActionBar;
    private final SiteIdentityPopup mIdentityPopup;
    private final ImageButton mIconView;
    private final TextView mTitleView;
    private final TextView mUrlView;
    private final Handler mHandler = new Handler();

    private Runnable mUpdateAction;

    @ColorInt
    private int mTextPrimaryColor = DEFAULT_TEXT_PRIMARY_COLOR;

    ActionBarPresenter(@NonNull final ActionBar actionBar) {
        mActionBar = actionBar;
        mActionBar.setDisplayShowCustomEnabled(true);
        mActionBar.setDisplayShowTitleEnabled(false);

        mActionBar.setCustomView(R.layout.customtabs_action_bar_custom_view);
        final View customView = mActionBar.getCustomView();
        mIconView = (ImageButton) customView.findViewById(R.id.custom_tabs_action_bar_icon);
        mTitleView = (TextView) customView.findViewById(R.id.custom_tabs_action_bar_title);
        mUrlView = (TextView) customView.findViewById(R.id.custom_tabs_action_bar_url);

        onThemeChanged(mActionBar.getThemedContext().getTheme());

        mIdentityPopup = new SiteIdentityPopup(mActionBar.getThemedContext());
        mIdentityPopup.setAnchor(customView);
        mIconView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mIdentityPopup.show();
            }
        });
    }

    /**
     * To display Url in CustomView only and immediately.
     *
     * @param url Url String to display
     */
    public void displayUrlOnly(@NonNull final String url) {
        updateCustomView(null, null, url);
    }

    /**
     * Update appearance of CustomView of ActionBar.
     *
     * @param tab a Tab instance of current web page to provide information to render ActionBar.
     */
    public void update(@NonNull final Tab tab) {
        final String title = tab.getTitle();
        final String url = tab.getBaseDomain();

        // Do not update CustomView immediately. If this method be invoked rapidly several times,
        // only apply last one.
        mHandler.removeCallbacks(mUpdateAction);
        mUpdateAction = new Runnable() {
            @Override
            public void run() {
                updateCustomView(tab.getSiteIdentity(), title, url);
            }
        };
        mHandler.postDelayed(mUpdateAction, CUSTOM_VIEW_UPDATE_DELAY);
    }

    /**
     * Set background color to ActionBar, as well as Status bar.
     *
     * @param color  the color to apply to ActionBar
     * @param window Window instance for changing color status bar, or null if won't change it.
     */
    @UiThread
    public void setBackgroundColor(@ColorInt final int color,
                                   @Nullable final Window window) {
        mActionBar.setBackgroundDrawable(new ColorDrawable(color));

        if (window != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
                window.setStatusBarColor(ColorUtil.darken(color, 0.25));
            }
        }
    }

    /**
     * To get primary color of Title of ActionBar
     *
     * @return color code of primary color
     */
    @ColorInt
    public int getTextPrimaryColor() {
        return mTextPrimaryColor;
    }

    /**
     * To update appearance of CustomView of ActionBar, includes its icon, title and url text.
     *
     * @param identity SiteIdentity for current website. Could be null if don't want to show icon.
     * @param title    Title for current website. Could be null if don't want to show title.
     * @param url      URL for current website. At least Custom will show this url.
     */
    @UiThread
    private void updateCustomView(@Nullable SiteIdentity identity,
                                  @Nullable String title,
                                  @NonNull String url) {
        // update site-info icon
        if (identity == null) {
            mIconView.setVisibility(View.INVISIBLE);
        } else {
            final SecurityModeUtil.Mode mode = SecurityModeUtil.resolve(identity);
            mIconView.setVisibility(View.VISIBLE);
            mIconView.setImageLevel(mode.ordinal());
            mIdentityPopup.setSiteIdentity(identity);

            if (mode == SecurityModeUtil.Mode.LOCK_SECURE) {
                // Lock-Secure is special case. Keep its original green color.
                DrawableCompat.setTintList(mIconView.getDrawable(), null);
            } else {
                // Icon use same color as TextView.
                DrawableCompat.setTint(mIconView.getDrawable(), mTextPrimaryColor);
            }
        }

        // If no title to use, use Url as title
        if (TextUtils.isEmpty(title)) {
            mTitleView.setText(url);
            mUrlView.setText(null);
            mUrlView.setVisibility(View.GONE);
        } else {
            mTitleView.setText(title);
            mUrlView.setText(url);
            mUrlView.setVisibility(View.VISIBLE);
        }
    }

    private void onThemeChanged(@NonNull final Resources.Theme currentTheme) {
        // Theme might be light or dark. To get text color for custom-view.
        final TypedArray themeArray = currentTheme.obtainStyledAttributes(
                new int[]{android.R.attr.textColorPrimary});

        mTextPrimaryColor = themeArray.getColor(0, DEFAULT_TEXT_PRIMARY_COLOR);
        themeArray.recycle();
    }
}
