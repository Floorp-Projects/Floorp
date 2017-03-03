/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.support.v7.app.ActionBar;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.util.ColorUtil;

import java.lang.reflect.Field;

/**
 * This class is used to maintain appearance of ActionBar of CustomTabsActivity, includes background
 * color, custom-view and so on.
 */
public class ActionBarPresenter {

    private static final String LOGTAG = "CustomTabsActionBar";
    private final ActionBar mActionBar;
    private boolean useDomainTitle = true;

    ActionBarPresenter(@NonNull final ActionBar actionBar, @NonNull Toolbar toolbar) {
        mActionBar = actionBar;
        initActionBar(toolbar);
    }

    private void initActionBar(@NonNull final Toolbar toolbar) {
        try {
            // Since we don't create the Toolbar's TextView ourselves, this seems
            // to be the only way of changing the ellipsize setting.
            final Field f = toolbar.getClass().getDeclaredField("mTitleTextView");
            f.setAccessible(true);
            final TextView textView = (TextView) f.get(toolbar);
            textView.setEllipsize(TextUtils.TruncateAt.START);
        } catch (Exception e) {
            // If we can't ellipsize at the start of the title, we shouldn't display the host
            // so as to avoid displaying a misleadingly truncated host.
            Log.w(LOGTAG, "Failed to get Toolbar TextView, using default title.");
            useDomainTitle = false;
        }
    }

    /**
     * Update appearance of ActionBar, includes its Title.
     *
     * @param title A string to be used as Title in Actionbar
     */
    @UiThread
    public void update(@Nullable final String title) {
        if (useDomainTitle || TextUtils.isEmpty(title)) {
            mActionBar.setTitle(AppConstants.MOZ_APP_BASENAME);
        } else {
            mActionBar.setTitle(title);
        }
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
}
