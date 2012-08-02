/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.animation.TranslateAnimation;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;
import android.widget.TextSwitcher;
import android.widget.TextView;
import android.widget.ViewSwitcher;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class BrowserToolbar implements ViewSwitcher.ViewFactory,
                                       Tabs.OnTabsChangedListener,
                                       GeckoMenu.ActionItemBarPresenter {
    private static final String LOGTAG = "GeckoToolbar";
    private LinearLayout mLayout;
    private Button mAwesomeBar;
    private ImageButton mTabs;
    private ImageView mBack;
    private ImageView mForward;
    public ImageButton mFavicon;
    public ImageButton mStop;
    public ImageButton mSiteSecurity;
    public ImageButton mReader;
    private AnimationDrawable mProgressSpinner;
    private TextSwitcher mTabsCount;
    private ImageView mShadow;
    private ImageButton mMenu;
    private LinearLayout mActionItemBar;
    private MenuPopup mMenuPopup;
    private List<View> mFocusOrder;

    final private BrowserApp mActivity;
    private LayoutInflater mInflater;
    private Handler mHandler;
    private int[] mPadding;
    private boolean mHasSoftMenuButton;

    private boolean mShowSiteSecurity;
    private boolean mShowReader;

    private static List<View> sActionItems;

    private int mDuration;
    private TranslateAnimation mSlideUpIn;
    private TranslateAnimation mSlideUpOut;
    private TranslateAnimation mSlideDownIn;
    private TranslateAnimation mSlideDownOut;

    private int mCount;

    private static final int TABS_CONTRACTED = 1;
    private static final int TABS_EXPANDED = 2;

    public BrowserToolbar(BrowserApp activity) {
        // BrowserToolbar is attached to BrowserApp only.
        mActivity = activity;
        mInflater = LayoutInflater.from(activity);

        sActionItems = new ArrayList<View>();
        Tabs.registerOnTabsChangedListener(this);
    }

    public void from(LinearLayout layout) {
        mLayout = layout;

        mShowSiteSecurity = false;
        mShowReader = false;

        mAwesomeBar = (Button) mLayout.findViewById(R.id.awesome_bar);
        mAwesomeBar.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                mActivity.autoHideTabs();
                onAwesomeBarSearch();
            }
        });
        mAwesomeBar.setOnCreateContextMenuListener(new View.OnCreateContextMenuListener() {
            public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
                MenuInflater inflater = mActivity.getMenuInflater();
                inflater.inflate(R.menu.titlebar_contextmenu, menu);

                String clipboard = GeckoAppShell.getClipboardText();
                if (clipboard == null || TextUtils.isEmpty(clipboard)) {
                    menu.findItem(R.id.pasteandgo).setVisible(false);
                    menu.findItem(R.id.paste).setVisible(false);
                }

                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                    String url = tab.getURL();
                    if (url == null) {
                        menu.findItem(R.id.copyurl).setVisible(false);
                        menu.findItem(R.id.share).setVisible(false);
                        menu.findItem(R.id.add_to_launcher).setVisible(false);
                    }
                } else {
                    // if there is no tab, remove anything tab dependent
                    menu.findItem(R.id.copyurl).setVisible(false);
                    menu.findItem(R.id.share).setVisible(false);
                    menu.findItem(R.id.add_to_launcher).setVisible(false);
                }
            }
        });

        mPadding = new int[] { mAwesomeBar.getPaddingLeft(),
                               mAwesomeBar.getPaddingTop(),
                               mAwesomeBar.getPaddingRight(),
                               mAwesomeBar.getPaddingBottom() };

        mTabs = (ImageButton) mLayout.findViewById(R.id.tabs);
        mTabs.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                toggleTabs();
            }
        });
        mTabs.setImageLevel(0);

        mTabsCount = (TextSwitcher) mLayout.findViewById(R.id.tabs_count);
        mTabsCount.removeAllViews();
        mTabsCount.setFactory(this);
        mTabsCount.setText("");
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

                RelativeLayout.LayoutParams iconsLayoutParams =
                        (RelativeLayout.LayoutParams) ((View) view.getParent()).getLayoutParams();

                // Calculate the left margin for the arrow based on the position of the lock icon.
                int leftMargin = lockLocation[0] - iconsLayoutParams.rightMargin;
                SiteIdentityPopup.getInstance().show(mSiteSecurity, leftMargin);
            }
        });

        mProgressSpinner = (AnimationDrawable) mActivity.getResources().getDrawable(R.drawable.progress_spinner);
        
        mStop = (ImageButton) mLayout.findViewById(R.id.stop);
        mStop.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null)
                    tab.doStop();
            }
        });

        mReader = (ImageButton) mLayout.findViewById(R.id.reader);
        mReader.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View view) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null)
                    tab.readerMode();
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

        mMenu = (ImageButton) mLayout.findViewById(R.id.menu);
        mActionItemBar = (LinearLayout) mLayout.findViewById(R.id.menu_items);
        mHasSoftMenuButton = !mActivity.hasPermanentMenuKey();

        if (mHasSoftMenuButton) {
            mMenu.setVisibility(View.VISIBLE);
            mMenu.setOnClickListener(new Button.OnClickListener() {
                public void onClick(View view) {
                    mActivity.openOptionsMenu();
                }
            });
        }

        if (Build.VERSION.SDK_INT >= 11) {
            View panel = mActivity.getMenuPanel();

            // If panel is null, the app is starting up for the first time;
            //    add this to the popup only if we have a soft menu button.
            // else, browser-toolbar is initialized on rotation,
            //    and we need to re-attach action-bar items.

            if (panel == null) {
                mActivity.onCreatePanelMenu(Window.FEATURE_OPTIONS_PANEL, null);
                panel = mActivity.getMenuPanel();

                if (mHasSoftMenuButton) {
                    mMenuPopup = new MenuPopup(mActivity);
                    mMenuPopup.setPanelView(panel);
                }
            }
        }

        mFocusOrder = Arrays.asList(mBack, mForward, mAwesomeBar, mReader, mSiteSecurity, mStop, mTabs);
    }

    public View getLayout() {
        return mLayout;
    }

    public void requestLayout() {
        mLayout.invalidate();
    }

    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch(msg) {
            case TITLE:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    setTitle(tab.getDisplayTitle());
                }
                break;
            case START:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    setSecurityMode(tab.getSecurityMode());
                    setReaderMode(tab.getReaderEnabled());
                    updateBackButton(tab.canDoBack());
                    updateForwardButton(tab.canDoForward());
                    Boolean showProgress = (Boolean)data;
                    if (showProgress && tab.getState() == Tab.STATE_LOADING)
                        setProgressVisibility(true);
                }
                break;
            case STOP:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    updateBackButton(tab.canDoBack());
                    updateForwardButton(tab.canDoForward());
                    setProgressVisibility(false);
                }
                break;
            case RESTORED:
            case SELECTED:
            case LOCATION_CHANGE:
            case LOAD_ERROR:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    refresh();
                }
                break;
            case CLOSED:
            case ADDED:
                updateTabCountAndAnimate(Tabs.getInstance().getCount());
                updateBackButton(false);
                updateForwardButton(false);
                break;
        }
    }

    @Override
    public View makeView() {
        // This returns a TextView for the TextSwitcher.
        return mInflater.inflate(R.layout.tabs_counter, null);
    }

    private void onAwesomeBarSearch() {
        mActivity.onSearchRequested();
    }

    private void addTab() {
        mActivity.addTab();
    }

    private void toggleTabs() {
        if (mActivity.areTabsShown()) {
            if (mActivity.hasTabsSideBar())
                mActivity.hideTabs();
        } else {
            // hide the virtual keyboard
            InputMethodManager imm =
                    (InputMethodManager) mActivity.getSystemService(Context.INPUT_METHOD_SERVICE);
            imm.hideSoftInputFromWindow(mTabs.getWindowToken(), 0);
            mActivity.showLocalTabs();
        }
    }

    public void updateTabCountAndAnimate(int count) {
        if (mCount > count) {
            mTabsCount.setInAnimation(mSlideDownIn);
            mTabsCount.setOutAnimation(mSlideDownOut);
        } else if (mCount < count) {
            mTabsCount.setInAnimation(mSlideUpIn);
            mTabsCount.setOutAnimation(mSlideUpOut);
        } else {
            return;
        }

        mTabsCount.setText(String.valueOf(count));
        mTabs.setContentDescription((count > 1) ?
                                    mActivity.getString(R.string.num_tabs, count) :
                                    mActivity.getString(R.string.one_tab));
        mCount = count;
        mHandler.postDelayed(new Runnable() {
            public void run() {
                ((TextView) mTabsCount.getCurrentView()).setTextColor(mActivity.getResources().getColor(R.color.url_bar_text_highlight));
            }
        }, mDuration);

        mHandler.postDelayed(new Runnable() {
            public void run() {
                ((TextView) mTabsCount.getCurrentView()).setTextColor(mActivity.getResources().getColor(R.color.tabs_counter_color));
            }
        }, 2 * mDuration);
    }

    public void updateTabCount(int count) {
        mTabsCount.setCurrentText(String.valueOf(count));
        mTabs.setContentDescription((count > 1) ?
                                    mActivity.getString(R.string.num_tabs, count) :
                                    mActivity.getString(R.string.one_tab));
        mCount = count;
        updateTabs(mActivity.areTabsShown());
    }

    public void updateTabs(boolean areTabsShown) {
        if (areTabsShown) {
            mTabs.getBackground().setLevel(TABS_EXPANDED);

            if (!mActivity.hasTabsSideBar()) {
                mTabs.setImageLevel(0);
                mTabsCount.setVisibility(View.GONE);
                mMenu.setImageLevel(TABS_EXPANDED);
                mMenu.getBackground().setLevel(TABS_EXPANDED);
            } else {
                mTabs.setImageLevel(TABS_EXPANDED);
            }
        } else {
            mTabs.setImageLevel(TABS_CONTRACTED);
            mTabs.getBackground().setLevel(TABS_CONTRACTED);

            if (!mActivity.hasTabsSideBar()) {
                mTabsCount.setVisibility(View.VISIBLE);
                mMenu.setImageLevel(TABS_CONTRACTED);
                mMenu.getBackground().setLevel(TABS_CONTRACTED);
            }
        }
    }

    public void setProgressVisibility(boolean visible) {
        if (visible) {
            mFavicon.setImageDrawable(mProgressSpinner);
            mProgressSpinner.start();
            setPageActionVisibility(true);
            Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - Throbber start");
        } else {
            mProgressSpinner.stop();
            setPageActionVisibility(false);
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab != null)
                setFavicon(selectedTab.getFavicon());
            Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - Throbber stop");
        }
    }

    public void setPageActionVisibility(boolean isLoading) {
        // Handle the loading mode page actions
        mStop.setVisibility(isLoading ? View.VISIBLE : View.GONE);

        // Handle the viewing mode page actions
        mSiteSecurity.setVisibility(mShowSiteSecurity && !isLoading ? View.VISIBLE : View.GONE);
        mReader.setVisibility(mShowReader && !isLoading ? View.VISIBLE : View.GONE);

        if (!isLoading && !mShowSiteSecurity && !mShowReader) {
            // No visible page actions
            mAwesomeBar.setPadding(mPadding[0], mPadding[1], mPadding[2], mPadding[3]);
        } else {
            // At least one visible page action
            mAwesomeBar.setPadding(mPadding[0], mPadding[1], mPadding[0], mPadding[3]);
        }

        updateFocusOrder();
    }

    private void updateFocusOrder() {
        View prevView = null;

        for (View view : mFocusOrder) {
            if (view.getVisibility() != View.VISIBLE)
                continue;

            if (prevView != null) {
                view.setNextFocusLeftId(prevView.getId());
                prevView.setNextFocusRightId(view.getId());
            }

            prevView = view;
        }
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
        mShowSiteSecurity = true;

        if (mode.equals(SiteIdentityPopup.IDENTIFIED)) {
            mSiteSecurity.setImageLevel(1);
        } else if (mode.equals(SiteIdentityPopup.VERIFIED)) {
            mSiteSecurity.setImageLevel(2);
        } else {
            mSiteSecurity.setImageLevel(0);
            mShowSiteSecurity = false;
        }

        setPageActionVisibility(mStop.getVisibility() == View.VISIBLE);
    }

    public void setReaderMode(boolean showReader) {
        mShowReader = showReader;
        setPageActionVisibility(mStop.getVisibility() == View.VISIBLE);
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

    @Override
    public void addActionItem(View actionItem) {
        mActionItemBar.addView(actionItem);

        if (!sActionItems.contains(actionItem))
            sActionItems.add(actionItem);
    }

    @Override
    public void removeActionItem(int index) {
        mActionItemBar.removeViewAt(index);
        sActionItems.remove(index);
    }

    @Override
    public int getActionItemsCount() {
        return sActionItems.size();
    }

    public void show() {
        mLayout.setVisibility(View.VISIBLE);
    }

    public void hide() {
        mLayout.setVisibility(View.GONE);
    }

    public void refresh() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            String url = tab.getURL();
            setTitle(tab.getDisplayTitle());
            setFavicon(tab.getFavicon());
            setSecurityMode(tab.getSecurityMode());
            setReaderMode(tab.getReaderEnabled());
            setProgressVisibility(tab.getState() == Tab.STATE_LOADING);
            setShadowVisibility((url == null) || !url.startsWith("about:"));
            updateTabCount(Tabs.getInstance().getCount());
            updateBackButton(tab.canDoBack());
            updateForwardButton(tab.canDoForward());
        }
    }

    public void destroy() {
        // The action-items views are reused on rotation.
        // Remove them from their parent, so they can be re-attached to new parent.
        mActionItemBar.removeAllViews();
    }

    public boolean openOptionsMenu() {
        if (!mHasSoftMenuButton)
            return false;

        GeckoApp.mAppContext.invalidateOptionsMenu();
        if (mMenuPopup != null && !mMenuPopup.isShowing())
            mMenuPopup.showAsDropDown(mMenu);

        return true;
    }

    public boolean closeOptionsMenu() {
        if (!mHasSoftMenuButton)
            return false;

        if (mMenuPopup != null && mMenuPopup.isShowing())
            mMenuPopup.dismiss();

        return true;
    }

    // MenuPopup holds the MenuPanel in Honeycomb/ICS devices with no hardware key
    public class MenuPopup extends PopupWindow {
        private RelativeLayout mPanel;

        public MenuPopup(Context context) {
            super(context);
            setFocusable(true);

            // Setting a null background makes the popup to not close on touching outside.
            setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            setWindowLayoutMode(ViewGroup.LayoutParams.WRAP_CONTENT,
                                ViewGroup.LayoutParams.WRAP_CONTENT);

            LayoutInflater inflater = LayoutInflater.from(context);
            RelativeLayout layout = (RelativeLayout) inflater.inflate(R.layout.menu_popup, null);
            setContentView(layout);

            mPanel = (RelativeLayout) layout.findViewById(R.id.menu_panel);
        }

        public void setPanelView(View view) {
            mPanel.removeAllViews();
            mPanel.addView(view);
        }
    }
}
