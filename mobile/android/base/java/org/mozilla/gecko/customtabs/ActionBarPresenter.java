/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.MenuItemCompat;
import android.support.v7.app.ActionBar;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.GeckoSession.ProgressListener.SecurityInformation;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.toolbar.SecurityModeUtil;
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
    private final CustomTabsSecurityPopup mIdentityPopup;
    private final ImageButton mIconView;
    private final TextView mTitleView;
    private final TextView mUrlView;
    private final Handler mHandler = new Handler();

    private Runnable mUpdateAction;

    @ColorInt
    private int mTextPrimaryColor = DEFAULT_TEXT_PRIMARY_COLOR;

    ActionBarPresenter(@NonNull final ActionBar actionBar, @ColorInt final int textColor) {
        mTextPrimaryColor = textColor;
        mActionBar = actionBar;
        mActionBar.setDisplayShowCustomEnabled(true);
        mActionBar.setDisplayShowTitleEnabled(false);

        mActionBar.setCustomView(R.layout.customtabs_action_bar_custom_view);
        final View customView = mActionBar.getCustomView();
        mIconView = (ImageButton) customView.findViewById(R.id.custom_tabs_action_bar_icon);
        mTitleView = (TextView) customView.findViewById(R.id.custom_tabs_action_bar_title);
        mUrlView = (TextView) customView.findViewById(R.id.custom_tabs_action_bar_url);

        mTitleView.setTextColor(mTextPrimaryColor);
        mUrlView.setTextColor(mTextPrimaryColor);

        mIdentityPopup = new CustomTabsSecurityPopup(mActionBar.getThemedContext());
        mIdentityPopup.setAnchor(customView);
        mIconView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mIdentityPopup.show();
            }
        });

        initIndicator();
    }

    /**
     * To display Url in CustomView only and immediately.
     *
     * @param url Url String to display
     */
    public void displayUrlOnly(@NonNull final String url) {
        updateCustomView(null, url, /* security */ null);
    }

    /**
     * Update appearance of CustomView of ActionBar
     *
     * @param title          Title for current website. Could be null if don't want to show title.
     * @param url            URL for current website. At least Custom will show this url.
     * @param security       A SecurityInformation object giving the current security information
     */
    public void update(final String title, final String url, final SecurityInformation security) {
        // Do not update CustomView immediately. If this method be invoked rapidly several times,
        // only apply last one.
        mHandler.removeCallbacks(mUpdateAction);
        mUpdateAction = new Runnable() {
            @Override
            public void run() {
                updateCustomView(title, url, security);
            }
        };
        mHandler.postDelayed(mUpdateAction, CUSTOM_VIEW_UPDATE_DELAY);
    }

    /**
     * To add a always-show-as-action button to menu, and manually create a view to insert.
     *
     * @param menu the menu to insert new action-button.
     * @param icon the image to be used for action-button.
     * @param tint true if the icon should be tint by primary text color
     * @return the view which be inserted to menu
     */
    public View addActionButton(@NonNull final Menu menu,
                                @NonNull final Drawable icon,
                                final boolean tint) {
        final Resources res = mActionBar.getThemedContext().getResources();

        // if we specify layout_width, layout_height in xml to build View, then add to ActionBar
        // system might overwrite the value to WRAP_CONTENT.
        // Therefore we create view manually to match design spec
        final ImageButton btn = new ImageButton(mActionBar.getThemedContext(), null, 0);
        final int size = res.getDimensionPixelSize(R.dimen.custom_tab_action_button_size);
        final int padding = res.getDimensionPixelSize(R.dimen.custom_tab_action_button_padding);
        btn.setPadding(padding, padding, padding, padding);
        btn.setLayoutParams(new ViewGroup.LayoutParams(size, size));
        btn.setScaleType(ImageView.ScaleType.FIT_CENTER);

        if (tint) {
            Drawable wrapped = DrawableCompat.wrap(icon);
            DrawableCompat.setTint(wrapped, mTextPrimaryColor);
            btn.setImageDrawable(wrapped);
        } else {
            btn.setImageDrawable(icon);
        }

        // menu id does not matter here. We can directly bind callback to the returned-view.
        final MenuItem item = menu.add(Menu.NONE, -1, Menu.NONE, "");
        item.setActionView(btn);
        MenuItemCompat.setShowAsAction(item, MenuItemCompat.SHOW_AS_ACTION_ALWAYS);
        return btn;
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
     * To assign a long-click-listener to text area of ActionBar
     *
     * @param listener then callback to trigger
     */
    public void setTextLongClickListener(View.OnLongClickListener listener) {
        mTitleView.setOnLongClickListener(listener);
        mUrlView.setOnLongClickListener(listener);
    }

    private void initIndicator() {
        mActionBar.setDisplayHomeAsUpEnabled(true);

        @SuppressWarnings("deprecation")
        final Drawable icon = mActionBar.getThemedContext()
                .getResources()
                .getDrawable(R.drawable.close);

        Drawable wrapped = DrawableCompat.wrap(icon);
        DrawableCompat.setTint(wrapped, mTextPrimaryColor);
        mActionBar.setHomeAsUpIndicator(wrapped);
    }

    /**
     * To update appearance of CustomView of ActionBar, includes its icon, title and url text.
     *
     * @param title    Title for current website. Could be null if don't want to show title.
     * @param url      URL for current website. At least Custom will show this url.
     * @param security A SecurityInformation object giving the current security information
     */
    @UiThread
    private void updateCustomView(final String title, final String url, final SecurityInformation security) {
        if (security == null) {
            mIconView.setVisibility(View.INVISIBLE);
        } else {
            SecurityModeUtil.IconType icon;
            if (SecurityInformation.SECURITY_MODE_UNKNOWN == security.securityMode) {
                icon = SecurityModeUtil.IconType.UNKNOWN;
            } else {
                icon = SecurityModeUtil.IconType.LOCK_SECURE;
            }

            if (SecurityInformation.CONTENT_LOADED == security.mixedModePassive) {
                icon = SecurityModeUtil.IconType.WARNING;
            }

            mIconView.setVisibility(View.VISIBLE);
            mIconView.setImageLevel(icon.getImageLevel());
            mIdentityPopup.setSecurityInformation(security);

            if (icon == SecurityModeUtil.IconType.LOCK_SECURE) {
                // Lock-Secure is a special case. Keep its original green color.
                DrawableCompat.setTintList(mIconView.getDrawable(), null);
            } else {
                // Icon uses same color as TextView.
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
}
