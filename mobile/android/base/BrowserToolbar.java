/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;
import android.view.animation.TranslateAnimation;
import android.view.Gravity;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.TextView;
import android.widget.TextSwitcher;
import android.widget.ViewSwitcher;

public class BrowserToolbar implements ViewSwitcher.ViewFactory {
    private static final String LOGTAG = "GeckoToolbar";
    private LinearLayout mLayout;
    private Button mAwesomeBar;
    private ImageButton mTabs;
    private ImageView mBack;
    private ImageView mForward;
    public ImageButton mFavicon;
    public ImageButton mStop;
    public ImageButton mSiteSecurity;
    private AnimationDrawable mProgressSpinner;
    private TextSwitcher mTabsCount;
    private ImageView mShadow;
    private LayoutInflater mInflater;

    final private Context mContext;
    private Handler mHandler;
    private int mColor;
    private int[] mPadding;
    private boolean mTitleCanExpand;

    private int mDuration;
    private TranslateAnimation mSlideUpIn;
    private TranslateAnimation mSlideUpOut;
    private TranslateAnimation mSlideDownIn;
    private TranslateAnimation mSlideDownOut;

    private int mCount;

    public BrowserToolbar(Context context) {
        mContext = context;
        mInflater = LayoutInflater.from(context);
    }

    public void from(LinearLayout layout) {
        mLayout = layout;
        mTitleCanExpand = true;

        // Get the device's highlight color
        TypedArray typedArray;

        if (Build.VERSION.SDK_INT >= 11) {            
            typedArray = mContext.obtainStyledAttributes(new int[] { android.R.attr.textColorHighlight });
        } else {
            ContextThemeWrapper wrapper  = new ContextThemeWrapper(mContext, android.R.style.TextAppearance);
            typedArray = wrapper.getTheme().obtainStyledAttributes(new int[] { android.R.attr.textColorHighlight });
        }

        mColor = typedArray.getColor(typedArray.getIndex(0), 0);
        typedArray.recycle();
        mAwesomeBar = (Button) mLayout.findViewById(R.id.awesome_bar);
        mAwesomeBar.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                onAwesomeBarSearch();
            }
        });

        Resources resources = mContext.getResources();
        
        mPadding = new int[] { mAwesomeBar.getPaddingLeft(),
                               mAwesomeBar.getPaddingTop(),
                               mAwesomeBar.getPaddingRight(),
                               mAwesomeBar.getPaddingBottom() };

        GeckoStateListDrawable states = new GeckoStateListDrawable();
        states.initializeFilter(mColor);
        states.addState(new int[] { android.R.attr.state_pressed }, resources.getDrawable(R.drawable.address_bar_url_pressed));
        states.addState(new int[] { }, resources.getDrawable(R.drawable.address_bar_url_default));
        mAwesomeBar.setBackgroundDrawable(states);

        mAwesomeBar.setPadding(mPadding[0], mPadding[1], mPadding[2], mPadding[3]);

        mTabs = (ImageButton) mLayout.findViewById(R.id.tabs);
        mTabs.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                if (Tabs.getInstance().getCount() > 1)
                    showTabs();
                else
                    addTab();
            }
        });
        mTabs.setImageLevel(0);

        mTabsCount = (TextSwitcher) mLayout.findViewById(R.id.tabs_count);
        mTabsCount.removeAllViews();
        mTabsCount.setFactory(this);
        mTabsCount.setText("0");
        mCount = 0;

        mBack = (ImageButton) mLayout.findViewById(R.id.back);
        mBack.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View view) {
                Tabs.getInstance().getSelectedTab().doBack();
            }
        });

        mForward = (ImageButton) mLayout.findViewById(R.id.forward);
        mForward.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View view) {
                Tabs.getInstance().getSelectedTab().doForward();
            }
        });

        mFavicon = (ImageButton) mLayout.findViewById(R.id.favicon);
        mSiteSecurity = (ImageButton) mLayout.findViewById(R.id.site_security);
        mSiteSecurity.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View view) {
                int[] lockLocation = new int[2];
                view.getLocationOnScreen(lockLocation);
                LayoutParams lockLayoutParams = (LayoutParams) view.getLayoutParams();

                // Calculate the left margin for the arrow based on the position of the lock icon.
                int leftMargin = lockLocation[0] - lockLayoutParams.rightMargin;
                SiteIdentityPopup.getInstance().show(leftMargin);
            }
        });

        mProgressSpinner = (AnimationDrawable) resources.getDrawable(R.drawable.progress_spinner);
        
        mStop = (ImageButton) mLayout.findViewById(R.id.stop);
        mStop.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null)
                    tab.doStop();
            }
        });

        mShadow = (ImageView) mLayout.findViewById(R.id.shadow);

        mHandler = new Handler();
        mSlideUpIn = new TranslateAnimation(0, 0, 40, 0);
        mSlideUpOut = new TranslateAnimation(0, 0, 0, -40);
        mSlideDownIn = new TranslateAnimation(0, 0, -40, 0);
        mSlideDownOut = new TranslateAnimation(0, 0, 0, 40);

        mDuration = 750;
        mSlideUpIn.setDuration(mDuration);
        mSlideUpOut.setDuration(mDuration);
        mSlideDownIn.setDuration(mDuration);
        mSlideDownOut.setDuration(mDuration);
    }

    @Override
    public View makeView() {
        // This returns a TextView for the TextSwitcher.
        return mInflater.inflate(R.layout.tabs_counter, null);
    }

    private void onAwesomeBarSearch() {
        GeckoApp.mAppContext.onSearchRequested();
    }

    private void addTab() {
        GeckoApp.mAppContext.addTab();
    }

    private void showTabs() {
        GeckoApp.mAppContext.showTabs();
    }

    public int getHighlightColor() {
        return mColor;
    }

    public void updateTabCountAndAnimate(int count) {
        if (mCount > count) {
            mTabsCount.setInAnimation(mSlideDownIn);
            mTabsCount.setOutAnimation(mSlideDownOut);
        } else if (mCount < count) {
            mTabsCount.setInAnimation(mSlideUpIn);
            mTabsCount.setOutAnimation(mSlideUpOut);
        }

        // Always update the count text even if we're not showing it,
        // since it can appear in a future animation (e.g. 1 -> 2)
        mTabsCount.setText(String.valueOf(count));
        mCount = count;

        if (count > 1) {
            // Show tab count if it is greater than 1
            mTabsCount.setVisibility(View.VISIBLE);
            // Set image to more tabs dropdown "v"
            mTabs.setImageLevel(count);
            mTabs.setContentDescription(mContext.getString(R.string.num_tabs, count));
        }

        mHandler.postDelayed(new Runnable() {
            public void run() {
                ((TextView) mTabsCount.getCurrentView()).setTextColor(mColor);
            }
        }, mDuration);

        mHandler.postDelayed(new Runnable() {
            public void run() {
                // This will only happen when we are animating from 2 -> 1.
                // We're doing this here (as opposed to above) because we want
                // the count to disappear _after_ the animation.
                if (Tabs.getInstance().getCount() == 1) {
                    // Set image to new tab button "+"
                    mTabs.setImageLevel(1);
                    mTabsCount.setVisibility(View.GONE);
                    mTabs.setContentDescription(mContext.getString(R.string.new_tab));
                }
                ((TextView) mTabsCount.getCurrentView()).setTextColor(mContext.getResources().getColor(R.color.tabs_counter_color));
            }
        }, 2 * mDuration);
    }

    public void updateTabCount(int count) {
        mTabsCount.setCurrentText(String.valueOf(count));
        mTabs.setImageLevel(count);
        if (count > 1) {
            mTabsCount.setVisibility(View.VISIBLE);
            mTabs.setContentDescription(mContext.getString(R.string.num_tabs, count));
        } else {
            mTabsCount.setVisibility(View.INVISIBLE);
            mTabs.setContentDescription(mContext.getString(R.string.new_tab));
        }
    }

    public void setProgressVisibility(boolean visible) {
        if (visible) {
            mFavicon.setImageDrawable(mProgressSpinner);
            mProgressSpinner.start();
            setStopVisibility(true);
            Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - Throbber start");
        } else {
            mProgressSpinner.stop();
            setStopVisibility(false);
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab != null)
                setFavicon(selectedTab.getFavicon());
            Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - Throbber stop");
        }
    }

    public void setStopVisibility(boolean visible) {
        mStop.setVisibility(visible ? View.VISIBLE : View.GONE);
        mSiteSecurity.setVisibility(visible ? View.GONE : View.VISIBLE);
        if (!visible && mTitleCanExpand)
            mAwesomeBar.setPadding(mPadding[0], mPadding[1], mPadding[2], mPadding[3]);
        else
            mAwesomeBar.setPadding(mPadding[0], mPadding[1], mPadding[0], mPadding[3]);
    }

    public void setShadowVisibility(boolean visible) {
        mShadow.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    public void setTitle(CharSequence title) {
        Tab tab = Tabs.getInstance().getSelectedTab();

        // We use about:empty as a placeholder for an external page load and
        // we don't want to change the title
        if (tab != null && "about:empty".equals(tab.getURL()))
            return;

        // Setting a null title for about:home will ensure we just see
        // the "Enter Search or Address" placeholder text
        if (tab != null && "about:home".equals(tab.getURL()))
            title = null;

        mAwesomeBar.setText(title);
    }

    public void setFavicon(Drawable image) {
        if (Tabs.getInstance().getSelectedTab().getState() == Tab.STATE_LOADING)
            return;

        if (image != null)
            mFavicon.setImageDrawable(image);
        else
            mFavicon.setImageResource(R.drawable.favicon);
    }
    
    public void setSecurityMode(String mode) {
        mTitleCanExpand = false;

        if (mode.equals(SiteIdentityPopup.IDENTIFIED)) {
            mSiteSecurity.setImageLevel(1);
        } else if (mode.equals(SiteIdentityPopup.VERIFIED)) {
            mSiteSecurity.setImageLevel(2);
        } else {
            mSiteSecurity.setImageLevel(0);
            mTitleCanExpand = true;
        }
    }

    public void setVisibility(int visibility) {
        mLayout.setVisibility(visibility);
    }

    public void requestFocusFromTouch() {
        mLayout.requestFocusFromTouch();
    }

    public void updateBackButton(boolean enabled) {
         mBack.setColorFilter(enabled ? 0 : 0xFF999999);
         mBack.setEnabled(enabled);
    }

    public void updateForwardButton(boolean enabled) {
         mForward.setColorFilter(enabled ? 0 : 0xFF999999);
         mForward.setEnabled(enabled);
    }

    public void show() {
        if (Build.VERSION.SDK_INT >= 11)
            GeckoActionBar.show(GeckoApp.mAppContext);
        else
            mLayout.setVisibility(View.VISIBLE);
    }

    public void hide() {
        if (Build.VERSION.SDK_INT >= 11)
            GeckoActionBar.hide(GeckoApp.mAppContext);
        else
            mLayout.setVisibility(View.GONE);
    }

    public void refresh() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            String url = tab.getURL();
            setTitle(tab.getDisplayTitle());
            setFavicon(tab.getFavicon());
            setSecurityMode(tab.getSecurityMode());
            setProgressVisibility(tab.getState() == Tab.STATE_LOADING);
            setShadowVisibility((url == null) || !url.startsWith("about:"));
            updateTabCount(Tabs.getInstance().getCount());
            updateBackButton(tab.canDoBack());
            updateForwardButton(tab.canDoForward());
        }
    }
}
