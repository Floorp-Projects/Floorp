/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.graphics.Rect;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.Window;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.TranslateAnimation;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.TextView;
import android.widget.ViewSwitcher;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class BrowserToolbar implements ViewSwitcher.ViewFactory,
                                       Tabs.OnTabsChangedListener,
                                       GeckoMenu.ActionItemBarPresenter,
                                       Animation.AnimationListener {
    private static final String LOGTAG = "GeckoToolbar";
    private LinearLayout mLayout;
    private View mAwesomeBar;
    private LayoutParams mAwesomeBarParams;
    private View mAwesomeBarEntry;
    private int mAwesomeBarEntryRightMargin;
    private GeckoFrameLayout mAwesomeBarRightEdge;
    private BrowserToolbarBackground mAddressBarBg;
    private BrowserToolbarBackground.CurveTowards mAddressBarBgCurveTowards;
    private int mAddressBarBgRightMargin;
    private GeckoTextView mTitle;
    private int mTitlePadding;
    private boolean mSiteSecurityVisible;
    private boolean mAnimateSiteSecurity;
    private GeckoImageButton mTabs;
    private int mTabsPaneWidth;
    private ImageButton mBack;
    private ImageButton mForward;
    public ImageButton mFavicon;
    public ImageButton mStop;
    public ImageButton mSiteSecurity;
    public ImageButton mReader;
    private AnimationDrawable mProgressSpinner;
    private GeckoTextSwitcher mTabsCount;
    private ImageView mShadow;
    private GeckoImageButton mMenu;
    private LinearLayout mActionItemBar;
    private MenuPopup mMenuPopup;
    private List<View> mFocusOrder;

    final private BrowserApp mActivity;
    private LayoutInflater mInflater;
    private Handler mHandler;
    private boolean mHasSoftMenuButton;

    private boolean mShowSiteSecurity;
    private boolean mShowReader;

    private static List<View> sActionItems;

    private int mDuration;
    private TranslateAnimation mSlideUpIn;
    private TranslateAnimation mSlideUpOut;
    private TranslateAnimation mSlideDownIn;
    private TranslateAnimation mSlideDownOut;

    private AlphaAnimation mLockFadeIn;
    private TranslateAnimation mTitleSlideLeft;
    private TranslateAnimation mTitleSlideRight;

    private int mCount;
    private int mFaviconSize;

    private static final int TABS_CONTRACTED = 1;
    private static final int TABS_EXPANDED = 2;

    public BrowserToolbar(BrowserApp activity) {
        // BrowserToolbar is attached to BrowserApp only.
        mActivity = activity;
        mInflater = LayoutInflater.from(activity);

        sActionItems = new ArrayList<View>();
        Tabs.registerOnTabsChangedListener(this);
        mAnimateSiteSecurity = true;
    }

    public void from(LinearLayout layout) {
        if (mLayout != null) {
            // make sure we retain the visibility property on rotation
            layout.setVisibility(mLayout.getVisibility());
        }
        mLayout = layout;

        mShowSiteSecurity = false;
        mShowReader = false;

        mAddressBarBg = (BrowserToolbarBackground) mLayout.findViewById(R.id.address_bar_bg);
        mAwesomeBarRightEdge = (GeckoFrameLayout) mLayout.findViewById(R.id.awesome_bar_right_edge);
        mAwesomeBarEntry = mLayout.findViewById(R.id.awesome_bar_entry);

        // This will hold the translation width inside the toolbar when the tabs
        // pane is visible. It will affect the padding applied to the title TextView.
        mTabsPaneWidth = 0;

        mTitle = (GeckoTextView) mLayout.findViewById(R.id.awesome_bar_title);
        mTitlePadding = mTitle.getPaddingRight();
        if (Build.VERSION.SDK_INT >= 16)
            mTitle.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);

        mAwesomeBar = mLayout.findViewById(R.id.awesome_bar);
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

        mTabs = (GeckoImageButton) mLayout.findViewById(R.id.tabs);
        mTabs.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                toggleTabs();
            }
        });
        mTabs.setImageLevel(0);

        mTabsCount = (GeckoTextSwitcher) mLayout.findViewById(R.id.tabs_count);
        mTabsCount.removeAllViews();
        mTabsCount.setFactory(this);
        mTabsCount.setText("");
        mCount = 0;
        if (Build.VERSION.SDK_INT >= 16) {
            // This adds the TextSwitcher to the a11y node tree, where we in turn
            // could make it return an empty info node. If we don't do this the
            // TextSwitcher's child TextViews get picked up, and we don't want
            // that since the tabs ImageButton is already properly labeled for
            // accessibility.
            mTabsCount.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
            mTabsCount.setAccessibilityDelegate(new View.AccessibilityDelegate() {
                    public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfo info) {}
                });
        }

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

        Button.OnClickListener faviconListener = new Button.OnClickListener() {
            public void onClick(View view) {
                if (mSiteSecurity.getVisibility() != View.VISIBLE)
                    return;

                SiteIdentityPopup.getInstance().show(mSiteSecurity);
            }
        };

        mFavicon = (ImageButton) mLayout.findViewById(R.id.favicon);
        mFavicon.setOnClickListener(faviconListener);
        if (Build.VERSION.SDK_INT >= 16)
            mFavicon.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        mFaviconSize = Math.round(mActivity.getResources().getDimension(R.dimen.browser_toolbar_favicon_size));

        mSiteSecurity = (ImageButton) mLayout.findViewById(R.id.site_security);
        mSiteSecurity.setOnClickListener(faviconListener);
        mSiteSecurityVisible = (mSiteSecurity.getVisibility() == View.VISIBLE);

        mProgressSpinner = (AnimationDrawable) mActivity.getResources().getDrawable(R.drawable.progress_spinner);
        
        mStop = (ImageButton) mLayout.findViewById(R.id.stop);
        mStop.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null)
                    tab.doStop();
                setProgressVisibility(false);
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

        float slideWidth = mActivity.getResources().getDimension(R.dimen.browser_toolbar_lock_width);

        LinearLayout.LayoutParams siteSecParams = (LinearLayout.LayoutParams) mSiteSecurity.getLayoutParams();
        final float scale = mActivity.getResources().getDisplayMetrics().density;
        slideWidth += (siteSecParams.leftMargin + siteSecParams.rightMargin) * scale + 0.5f;

        mLockFadeIn = new AlphaAnimation(0.0f, 1.0f);
        mLockFadeIn.setAnimationListener(this);

        mTitleSlideLeft = new TranslateAnimation(slideWidth, 0, 0, 0);
        mTitleSlideLeft.setAnimationListener(this);

        mTitleSlideRight = new TranslateAnimation(-slideWidth, 0, 0, 0);
        mTitleSlideRight.setAnimationListener(this);

        final int lockAnimDuration = 300;
        mLockFadeIn.setDuration(lockAnimDuration);
        mTitleSlideLeft.setDuration(lockAnimDuration);
        mTitleSlideRight.setDuration(lockAnimDuration);

        mMenu = (GeckoImageButton) mLayout.findViewById(R.id.menu);
        mActionItemBar = (LinearLayout) mLayout.findViewById(R.id.menu_items);
        mHasSoftMenuButton = !mActivity.hasPermanentMenuKey();

        if (mHasSoftMenuButton) {
            mMenu.setVisibility(View.VISIBLE);
            mMenu.setOnClickListener(new Button.OnClickListener() {
                public void onClick(View view) {
                    mActivity.openOptionsMenu();
                }
            });

            // Set a touch delegate to Tabs button, so the touch events on its tail
            // are passed to the menu button.
            mLayout.post(new Runnable() {
                @Override
                public void run() {
                    int height = mTabs.getHeight();
                    int width = mTabs.getWidth();
                    int tail = (width - height) / 2;
                    Rect bounds = new Rect(width - tail, 0, width, height);
                    mTabs.setTouchDelegate(new TailTouchDelegate(bounds, mMenu));
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

                    mMenuPopup.setOnDismissListener(new PopupWindow.OnDismissListener() {
                        public void onDismiss() {
                            mActivity.onOptionsMenuClosed(null);
                        }
                    });
                }
            }
        }

        mFocusOrder = Arrays.asList(mBack, mForward, mAwesomeBar, mReader, mSiteSecurity, mStop, mTabs);
    }

    public View getLayout() {
        return mLayout;
    }

    public void refreshBackground() {
        mAddressBarBg.requestLayout();

        if (mAwesomeBarRightEdge != null)
            mAwesomeBarRightEdge.requestLayout();
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
                    updateBackButton(tab.canDoBack());
                    updateForwardButton(tab.canDoForward());
                    Boolean showProgress = (Boolean)data;
                    if (showProgress && tab.getState() == Tab.STATE_LOADING)
                        setProgressVisibility(true);
                    setSecurityMode(tab.getSecurityMode());
                    setReaderMode(tab.getReaderEnabled());
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
                updateTabCount(Tabs.getInstance().getCount());
                break;
            case SELECTED:
                mAnimateSiteSecurity = false;
                // fall through
            case LOCATION_CHANGE:
            case LOAD_ERROR:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    refresh();
                }
                mAnimateSiteSecurity = true;
                break;
            case CLOSED:
            case ADDED:
                updateTabCountAndAnimate(Tabs.getInstance().getCount());
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    updateBackButton(tab.canDoBack());
                    updateForwardButton(tab.canDoForward());
                }
                break;
        }
    }

    @Override
    public void onAnimationStart(Animation animation) {
        if (animation.equals(mLockFadeIn)) {
            if (mSiteSecurityVisible)
                mSiteSecurity.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public void onAnimationRepeat(Animation animation) {
    }

    @Override
    public void onAnimationEnd(Animation animation) {
        if (animation.equals(mTitleSlideLeft)) {
            mSiteSecurity.setVisibility(View.GONE);
        } else if (animation.equals(mTitleSlideRight)) {
            mSiteSecurity.startAnimation(mLockFadeIn);
        }
    }

    @Override
    public View makeView() {
        // This returns a TextView for the TextSwitcher.
        return mInflater.inflate(R.layout.tabs_counter, null);
    }

    private int prepareAwesomeBarAnimation() {
        // Keep the entry highlighted during the animation
        mAwesomeBar.setSelected(true);

        // Expand the entry to fill all the horizontal space available during the
        // animation. The fake right edge will slide on top of it to give the effect
        // of expanding the entry.
        MarginLayoutParams entryParams = (MarginLayoutParams) mAwesomeBarEntry.getLayoutParams();
        mAwesomeBarEntryRightMargin = entryParams.rightMargin;
        entryParams.rightMargin = 0;
        mAwesomeBarEntry.requestLayout();

        // Remove any curves from the toolbar background and expand it to fill all
        // the horizontal space.
        MarginLayoutParams barParams = (MarginLayoutParams) mAddressBarBg.getLayoutParams();
        mAddressBarBgRightMargin = barParams.rightMargin;
        barParams.rightMargin = 0;
        mAddressBarBgCurveTowards = mAddressBarBg.getCurveTowards();
        mAddressBarBg.setCurveTowards(BrowserToolbarBackground.CurveTowards.NONE);
        mAddressBarBg.requestLayout();

        // If we don't have any menu_items, then we simply slide all elements on the
        // rigth side of the toolbar out of screen.
        int translation = mAwesomeBarEntryRightMargin;

        if (mActionItemBar.getVisibility() == View.VISIBLE) {
            // If the toolbar has action items (e.g. on the tablet UI), the translation will
            // be in relation to the left side of their container (i.e. mActionItemBar).
            MarginLayoutParams itemBarParams = (MarginLayoutParams) mActionItemBar.getLayoutParams();
            translation = itemBarParams.rightMargin + mActionItemBar.getWidth() - entryParams.leftMargin;

            // Expand the whole entry container to fill all the horizontal space available
            View awesomeBarParent = (View) mAwesomeBar.getParent();
            mAwesomeBarParams = (LayoutParams) awesomeBarParent.getLayoutParams();
            awesomeBarParent.setLayoutParams(new LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                                              ViewGroup.LayoutParams.MATCH_PARENT));

            // Align the fake right edge to the right side of the entry bar
            MarginLayoutParams rightEdgeParams = (MarginLayoutParams) mAwesomeBarRightEdge.getLayoutParams();
            rightEdgeParams.rightMargin = itemBarParams.rightMargin + mActionItemBar.getWidth() - 100;
            mAwesomeBarRightEdge.requestLayout();
        }

        // Make the right edge visible to start the animation
        mAwesomeBarRightEdge.setVisibility(View.VISIBLE);

        return translation;
    }

    public void fromAwesomeBarSearch() {
        if (mActivity.hasTabsSideBar() || Build.VERSION.SDK_INT < 11) {
            return;
        }

        AnimatorProxy proxy = null;

        // If the awesomebar entry is not selected at this point, this means that
        // we had to reinflate the toolbar layout for some reason (device rotation
        // while in awesome screen, activity was killed in background, etc). In this
        // case, we have to ensure the toolbar is in the correct initial state to
        // shrink back.
        if (!mAwesomeBar.isSelected()) {
            int translation = prepareAwesomeBarAnimation();

            proxy = AnimatorProxy.create(mAwesomeBarRightEdge);
            proxy.setTranslationX(translation);
            proxy = AnimatorProxy.create(mTabs);
            proxy.setTranslationX(translation);
            proxy = AnimatorProxy.create(mTabsCount);
            proxy.setTranslationX(translation);
            proxy = AnimatorProxy.create(mMenu);
            proxy.setTranslationX(translation);
            proxy = AnimatorProxy.create(mActionItemBar);
            proxy.setTranslationX(translation);
        }

        // Restore opacity of content elements in the toolbar immediatelly
        // so that the response is immediate from user interaction in the
        // awesome screen.
        proxy = AnimatorProxy.create(mFavicon);
        proxy.setAlpha(1);
        proxy = AnimatorProxy.create(mSiteSecurity);
        proxy.setAlpha(1);
        proxy = AnimatorProxy.create(mTitle);
        proxy.setAlpha(1);
        proxy = AnimatorProxy.create(mForward);
        proxy.setAlpha(1);
        proxy = AnimatorProxy.create(mBack);
        proxy.setAlpha(1);

        final PropertyAnimator contentAnimator = new PropertyAnimator(250);

        // Shrink the awesome entry back to its original size
        contentAnimator.attach(mAwesomeBarRightEdge,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(mTabs,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(mTabsCount,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(mMenu,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(mActionItemBar,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);

        contentAnimator.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                mTabs.setVisibility(View.VISIBLE);
            }

            @Override
            public void onPropertyAnimationEnd() {
                // Turn off selected state on the entry
                mAwesomeBar.setSelected(false);

                // Restore entry state
                MarginLayoutParams entryParams = (MarginLayoutParams) mAwesomeBarEntry.getLayoutParams();
                entryParams.rightMargin = mAwesomeBarEntryRightMargin;
                mAwesomeBarEntry.requestLayout();

                // Restore the background state
                MarginLayoutParams barParams = (MarginLayoutParams) mAddressBarBg.getLayoutParams();
                barParams.rightMargin = mAddressBarBgRightMargin;
                mAddressBarBg.setCurveTowards(mAddressBarBgCurveTowards);
                mAddressBarBg.requestLayout();

                // If there are action bar items in the toolbar, we have to restore the
                // alignment of the entry in relation to them. mAwesomeBarParams might
                // be null if the activity holding the toolbar is killed before returning
                // from awesome screen (e.g. "Don't keep activities" is on)
                if (mActionItemBar.getVisibility() == View.VISIBLE)
                    ((View) mAwesomeBar.getParent()).setLayoutParams(mAwesomeBarParams);

                // Hide fake right edge, we only use for the animation
                mAwesomeBarRightEdge.setVisibility(View.INVISIBLE);

                PropertyAnimator buttonsAnimator = new PropertyAnimator(150);

                // Fade toolbar buttons (reader, stop) after the entry
                // is schrunk back to its original size.
                buttonsAnimator.attach(mReader,
                                       PropertyAnimator.Property.ALPHA,
                                       1);
                buttonsAnimator.attach(mStop,
                                       PropertyAnimator.Property.ALPHA,
                                       1);

                buttonsAnimator.start();
            }
        });

        mHandler.postDelayed(new Runnable() {
            public void run() {
                contentAnimator.start();
            }
        }, 500);
    }

    private void onAwesomeBarSearch() {
        // This animation doesn't make much sense in a sidebar UI
        if (mActivity.hasTabsSideBar() || Build.VERSION.SDK_INT < 11) {
            mActivity.onSearchRequested();
            return;
        }

        final PropertyAnimator contentAnimator = new PropertyAnimator(250);

        int translation = prepareAwesomeBarAnimation();

        if (mActionItemBar.getVisibility() == View.VISIBLE) {
            contentAnimator.attach(mFavicon,
                                   PropertyAnimator.Property.ALPHA,
                                   0);
            contentAnimator.attach(mSiteSecurity,
                                   PropertyAnimator.Property.ALPHA,
                                   0);
            contentAnimator.attach(mTitle,
                                   PropertyAnimator.Property.ALPHA,
                                   0);
        }

        // Fade out all controls inside the toolbar
        contentAnimator.attach(mForward,
                               PropertyAnimator.Property.ALPHA,
                               0);
        contentAnimator.attach(mBack,
                               PropertyAnimator.Property.ALPHA,
                               0);
        contentAnimator.attach(mReader,
                               PropertyAnimator.Property.ALPHA,
                               0);
        contentAnimator.attach(mStop,
                               PropertyAnimator.Property.ALPHA,
                               0);

        // Slide the right side elements of the toolbar
        contentAnimator.attach(mAwesomeBarRightEdge,
                               PropertyAnimator.Property.TRANSLATION_X,
                               translation);
        contentAnimator.attach(mTabs,
                               PropertyAnimator.Property.TRANSLATION_X,
                               translation);
        contentAnimator.attach(mTabsCount,
                               PropertyAnimator.Property.TRANSLATION_X,
                               translation);
        contentAnimator.attach(mMenu,
                               PropertyAnimator.Property.TRANSLATION_X,
                               translation);
        contentAnimator.attach(mActionItemBar,
                               PropertyAnimator.Property.TRANSLATION_X,
                               translation);

        contentAnimator.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                mTabs.setVisibility(View.INVISIBLE);

                // Once the entry is fully expanded, start awesome screen
                mActivity.onSearchRequested();
            }
        });

        contentAnimator.start();
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
                GeckoTextView view = (GeckoTextView) mTabsCount.getCurrentView();
                view.setSelected(true);
            }
        }, mDuration);

        mHandler.postDelayed(new Runnable() {
            public void run() {
                GeckoTextView view = (GeckoTextView) mTabsCount.getCurrentView();
                view.setSelected(false);
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

    public void prepareTabsAnimation(PropertyAnimator animator, int width) {
        // This is negative before we want to keep the right edge in the same
        // position while animating the left-most elements below.
        animator.attach(mAwesomeBarRightEdge,
                        PropertyAnimator.Property.TRANSLATION_X,
                        -width);

        animator.attach(mAwesomeBar,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        animator.attach(mAddressBarBg,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        animator.attach(mTabs,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        animator.attach(mTabsCount,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        animator.attach(mBack,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        animator.attach(mForward,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        animator.attach(mTitle,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        animator.attach(mFavicon,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);
        animator.attach(mSiteSecurity,
                        PropertyAnimator.Property.TRANSLATION_X,
                        width);

        // Uses the old mTabsPaneWidth.
        adjustTabsAnimation(false);

        mTabsPaneWidth = width;

        // Only update title padding immediatelly when shrinking the browser
        // toolbar. Leave the padding update to the end of the animation when
        // expanding (see finishTabsAnimation()).
        if (mTabsPaneWidth > 0)
            setPageActionVisibility(mStop.getVisibility() == View.VISIBLE);
    }

    public void adjustTabsAnimation(boolean reset) {
        int width = reset ? 0 : mTabsPaneWidth;
        mAwesomeBarRightEdge.setTranslationX(-width);
        mAwesomeBar.setTranslationX(width);
        mAddressBarBg.setTranslationX(width);
        mTabs.setTranslationX(width);
        mTabsCount.setTranslationX(width);
        mBack.setTranslationX(width);
        mForward.setTranslationX(width);
        mTitle.setTranslationX(width);
        mFavicon.setTranslationX(width);
        mSiteSecurity.setTranslationX(width);

        ((ViewGroup.MarginLayoutParams) mLayout.getLayoutParams()).leftMargin = reset ? mTabsPaneWidth : 0;
    }

    public void finishTabsAnimation() {
        setPageActionVisibility(mStop.getVisibility() == View.VISIBLE);
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

        // A level change will not trigger onMeasure() for the tabs, where the path is created.
        // Manually requesting a layout to re-calculate the path.
        mTabs.requestLayout();
    }

    public void setProgressVisibility(boolean visible) {
        // The "Throbber start" and "Throbber stop" log messages in this method
        // are needed by S1/S2 tests (http://mrcote.info/phonedash/#).
        // See discussion in Bug 804457. Bug 805124 tracks paring these down.
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
        setSiteSecurityVisibility(mShowSiteSecurity && !isLoading);
        mReader.setVisibility(mShowReader && !isLoading ? View.VISIBLE : View.GONE);

        // We want title to fill the whole space available for it when there are icons
        // being shown on the right side of the toolbar as the icons already have some
        // padding in them. This is just to avoid wasting space when icons are shown.
        mTitle.setPadding(0, 0, (!mShowReader && !isLoading ? mTitlePadding : 0), 0);

        updateFocusOrder();
    }

    private void setSiteSecurityVisibility(final boolean visible) {
        if (visible == mSiteSecurityVisible)
            return;

        mSiteSecurityVisible = visible;

        if (!mAnimateSiteSecurity) {
            mSiteSecurity.setVisibility(visible ? View.VISIBLE : View.GONE);
            return;
        }

        mTitle.clearAnimation();
        mSiteSecurity.clearAnimation();

        // If any of these animations were cancelled as a result of the
        // clearAnimation() calls above, we need to reset them.
        mLockFadeIn.reset();
        mTitleSlideLeft.reset();
        mTitleSlideRight.reset();

        if (visible)
            mSiteSecurity.setVisibility(View.INVISIBLE);
        else
            mSiteSecurity.setVisibility(View.GONE);

        mTitle.startAnimation(visible ? mTitleSlideRight : mTitleSlideLeft);
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
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null) {
            return;
        }

        String url = tab.getURL();

        // Only set shadow to visible when not on about screens.
        visible &= !(url == null || url.startsWith("about:"));

        if ((mShadow.getVisibility() == View.VISIBLE) != visible) {
            mShadow.setVisibility(visible ? View.VISIBLE : View.GONE);
        }
    }

    public void setTitle(CharSequence title) {
        Tab tab = Tabs.getInstance().getSelectedTab();

        // Keep the title unchanged if the tab is entering reader mode
        if (tab != null && tab.isEnteringReaderMode())
            return;

        // Setting a null title for about:home will ensure we just see
        // the "Enter Search or Address" placeholder text
        if (tab != null && "about:home".equals(tab.getURL()))
            title = null;

        mTitle.setText(title);
        mAwesomeBar.setContentDescription(title != null ? title : mTitle.getHint());
    }

    public void setFavicon(Bitmap image) {
        if (Tabs.getInstance().getSelectedTab().getState() == Tab.STATE_LOADING)
            return;

        if (image != null) {
            image = Bitmap.createScaledBitmap(image, mFaviconSize, mFaviconSize, false);
            mFavicon.setImageBitmap(image);
        } else {
            mFavicon.setImageResource(R.drawable.favicon);
        }
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
            setProgressVisibility(tab.getState() == Tab.STATE_LOADING);
            setSecurityMode(tab.getSecurityMode());
            setReaderMode(tab.getReaderEnabled());
            setShadowVisibility(true);
            updateTabCount(Tabs.getInstance().getCount());
            updateBackButton(tab.canDoBack());
            updateForwardButton(tab.canDoForward());

            mAddressBarBg.setPrivateMode(tab.isPrivate());

            if (mAwesomeBar instanceof GeckoButton)
                ((GeckoButton) mAwesomeBar).setPrivateMode(tab.isPrivate());
            else if (mAwesomeBar instanceof GeckoRelativeLayout)
                ((GeckoRelativeLayout) mAwesomeBar).setPrivateMode(tab.isPrivate());

            mTabs.setPrivateMode(tab.isPrivate());
            mTitle.setPrivateMode(tab.isPrivate());
            mMenu.setPrivateMode(tab.isPrivate());

            if (mBack instanceof BackButton)
                ((BackButton) mBack).setPrivateMode(tab.isPrivate());

            if (mForward instanceof ForwardButton)
                ((ForwardButton) mForward).setPrivateMode(tab.isPrivate());
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

    public static class RightEdge extends GeckoFrameLayout
                                  implements LightweightTheme.OnChangeListener { 
        private BrowserApp mActivity;

        public RightEdge(Context context, AttributeSet attrs) {
            super(context, attrs);
            mActivity = (BrowserApp) context;
        }

        @Override
        public void onAttachedToWindow() {
            super.onAttachedToWindow();
            mActivity.getLightweightTheme().addListener(this);
        }

        @Override
        public void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            mActivity.getLightweightTheme().removeListener(this);
        }

        @Override
        public void onLightweightThemeChanged() {
            Drawable drawable = mActivity.getLightweightTheme().getDrawable(this);
            if (drawable == null)
                return;

            StateListDrawable stateList = new StateListDrawable();
            stateList.addState(new int[] { R.attr.state_private }, mActivity.getResources().getDrawable(R.drawable.address_bar_bg_private));
            stateList.addState(new int[] {}, drawable);

            int[] padding =  new int[] { getPaddingLeft(),
                                         getPaddingTop(),
                                         getPaddingRight(),
                                         getPaddingBottom()
                                       };
            setBackgroundDrawable(stateList);
            setPadding(padding[0], padding[1], padding[2], padding[3]);
        }

        @Override
        public void onLightweightThemeReset() {
            int[] padding =  new int[] { getPaddingLeft(),
                                         getPaddingTop(),
                                         getPaddingRight(),
                                         getPaddingBottom()
                                       };
            setBackgroundResource(R.drawable.address_bar_bg);
            setPadding(padding[0], padding[1], padding[2], padding[3]);
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
            onLightweightThemeChanged();
        }
    }
}
