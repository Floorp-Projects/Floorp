/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.MenuPopup;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.HardwareUtils;

import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import org.mozilla.gecko.PrefsHelper;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.text.style.ForegroundColorSpan;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.Window;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.Animation;
import android.view.animation.AlphaAnimation;
import android.view.animation.Interpolator;
import android.view.animation.TranslateAnimation;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.ViewSwitcher;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class BrowserToolbar implements Tabs.OnTabsChangedListener,
                                       GeckoMenu.ActionItemBarPresenter,
                                       Animation.AnimationListener {
    private static final String LOGTAG = "GeckoToolbar";
    public static final String PREF_TITLEBAR_MODE = "browser.chrome.titlebarMode";
    private GeckoRelativeLayout mLayout;
    private LayoutParams mAwesomeBarParams;
    private View mUrlDisplayContainer;
    private View mAwesomeBarEntry;
    private ImageView mAwesomeBarRightEdge;
    private BrowserToolbarBackground mAddressBarBg;
    private GeckoTextView mTitle;
    private int mTitlePadding;
    private boolean mSiteSecurityVisible;
    private boolean mSwitchingTabs;
    private ShapedButton mTabs;
    private ImageButton mBack;
    private ImageButton mForward;
    public ImageButton mFavicon;
    public ImageButton mStop;
    public ImageButton mSiteSecurity;
    public ImageButton mReader;
    private AnimationDrawable mProgressSpinner;
    private TabCounter mTabsCounter;
    private ImageView mShadow;
    private GeckoImageButton mMenu;
    private GeckoImageView mMenuIcon;
    private LinearLayout mActionItemBar;
    private MenuPopup mMenuPopup;
    private List<View> mFocusOrder;

    final private BrowserApp mActivity;
    private Handler mHandler;
    private boolean mHasSoftMenuButton;

    private boolean mShowSiteSecurity;
    private boolean mShowReader;

    private static List<View> sActionItems;

    private boolean mAnimatingEntry;

    private AlphaAnimation mLockFadeIn;
    private TranslateAnimation mTitleSlideLeft;
    private TranslateAnimation mTitleSlideRight;

    private int mAddressBarViewOffset;
    private int mDefaultForwardMargin;
    private PropertyAnimator mForwardAnim = null;

    private int mFaviconSize;

    private PropertyAnimator mVisibilityAnimator;
    private static final Interpolator sButtonsInterpolator = new AccelerateInterpolator();

    private static final int TABS_CONTRACTED = 1;
    private static final int TABS_EXPANDED = 2;

    private static final int FORWARD_ANIMATION_DURATION = 450;
    private final ForegroundColorSpan mUrlColor;
    private final ForegroundColorSpan mDomainColor;
    private final ForegroundColorSpan mPrivateDomainColor;

    private boolean mShowUrl;

    private Integer mPrefObserverId;

    public BrowserToolbar(BrowserApp activity) {
        // BrowserToolbar is attached to BrowserApp only.
        mActivity = activity;

        sActionItems = new ArrayList<View>();
        Tabs.registerOnTabsChangedListener(this);
        mSwitchingTabs = true;

        mAnimatingEntry = false;
        mShowUrl = false;

        // listen to the title bar pref.
        mPrefObserverId = PrefsHelper.getPref(PREF_TITLEBAR_MODE, new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, String str) {
                int value = Integer.parseInt(str);
                boolean shouldShowUrl = (value == 1);

                if (shouldShowUrl == mShowUrl) {
                    return;
                }
                mShowUrl = shouldShowUrl;

                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        updateTitle();
                    }
                });
            }

            @Override
            public boolean isObserver() {
                // We want to be notified of changes to be able to switch mode
                // without restarting.
                return true;
            }
        });

        Resources res = mActivity.getResources();
        mUrlColor = new ForegroundColorSpan(res.getColor(R.color.url_bar_urltext));
        mDomainColor = new ForegroundColorSpan(res.getColor(R.color.url_bar_domaintext));
        mPrivateDomainColor = new ForegroundColorSpan(res.getColor(R.color.url_bar_domaintext_private));

    }

    public void from(RelativeLayout layout) {
        if (mLayout != null) {
            // make sure we retain the visibility property on rotation
            layout.setVisibility(mLayout.getVisibility());
        }

        mLayout = (GeckoRelativeLayout) layout;

        mLayout.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                mActivity.autoHideTabs();
                onAwesomeBarSearch();
            }
        });

        mLayout.setOnCreateContextMenuListener(new View.OnCreateContextMenuListener() {
            @Override
            public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
                MenuInflater inflater = mActivity.getMenuInflater();
                inflater.inflate(R.menu.titlebar_contextmenu, menu);

                String clipboard = Clipboard.getText();
                if (TextUtils.isEmpty(clipboard)) {
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
                    if (!tab.getFeedsEnabled()) {
                        menu.findItem(R.id.subscribe).setVisible(false);
                    }
                } else {
                    // if there is no tab, remove anything tab dependent
                    menu.findItem(R.id.copyurl).setVisible(false);
                    menu.findItem(R.id.share).setVisible(false);
                    menu.findItem(R.id.add_to_launcher).setVisible(false);
                    menu.findItem(R.id.subscribe).setVisible(false);
                }
            }
        });

        mShowSiteSecurity = false;
        mShowReader = false;

        mAnimatingEntry = false;

        mAddressBarBg = (BrowserToolbarBackground) mLayout.findViewById(R.id.address_bar_bg);
        mAddressBarViewOffset = mActivity.getResources().getDimensionPixelSize(R.dimen.addressbar_offset_left);
        mDefaultForwardMargin = mActivity.getResources().getDimensionPixelSize(R.dimen.forward_default_offset);
        mUrlDisplayContainer = mLayout.findViewById(R.id.awesome_bar_display_container);
        mAwesomeBarEntry = mLayout.findViewById(R.id.awesome_bar_entry);

        // This will clip the right edge's image at half of its width
        mAwesomeBarRightEdge = (ImageView) mLayout.findViewById(R.id.awesome_bar_right_edge);
        if (mAwesomeBarRightEdge != null) {
            mAwesomeBarRightEdge.getDrawable().setLevel(5000);
        }

        mTitle = (GeckoTextView) mLayout.findViewById(R.id.awesome_bar_title);
        mTitlePadding = mTitle.getPaddingRight();
        if (Build.VERSION.SDK_INT >= 16)
            mTitle.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);

        mTabs = (ShapedButton) mLayout.findViewById(R.id.tabs);
        mTabs.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleTabs();
            }
        });
        mTabs.setImageLevel(0);

        mTabsCounter = (TabCounter) mLayout.findViewById(R.id.tabs_counter);

        mBack = (ImageButton) mLayout.findViewById(R.id.back);
        mBack.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                Tabs.getInstance().getSelectedTab().doBack();
            }
        });
        mBack.setOnLongClickListener(new Button.OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                return Tabs.getInstance().getSelectedTab().showBackHistory();
            }
        });

        mForward = (ImageButton) mLayout.findViewById(R.id.forward);
        mForward.setEnabled(false); // initialize the forward button to not be enabled
        mForward.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                Tabs.getInstance().getSelectedTab().doForward();
            }
        });
        mForward.setOnLongClickListener(new Button.OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                return Tabs.getInstance().getSelectedTab().showForwardHistory();
            }
        });

        Button.OnClickListener faviconListener = new Button.OnClickListener() {
            @Override
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
            @Override
            public void onClick(View v) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null)
                    tab.doStop();
                setProgressVisibility(false);
            }
        });

        mReader = (ImageButton) mLayout.findViewById(R.id.reader);
        mReader.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                    tab.toggleReaderMode();
                }
            }
        });

        mReader.setOnLongClickListener(new Button.OnLongClickListener() {
            public boolean onLongClick(View v) {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                    tab.addToReadingList();
                    return true;
                }

                return false;
            }
        });

        mShadow = (ImageView) mLayout.findViewById(R.id.shadow);
        mShadow.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
            }
        });

        if (Build.VERSION.SDK_INT >= 16) {
            mShadow.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        }

        mHandler = new Handler();

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
        mMenuIcon = (GeckoImageView) mLayout.findViewById(R.id.menu_icon);
        mActionItemBar = (LinearLayout) mLayout.findViewById(R.id.menu_items);
        mHasSoftMenuButton = !HardwareUtils.hasMenuButton();

        if (mHasSoftMenuButton) {
            mMenu.setVisibility(View.VISIBLE);
            mMenuIcon.setVisibility(View.VISIBLE);

            mMenu.setOnClickListener(new Button.OnClickListener() {
                @Override
                public void onClick(View view) {
                    mActivity.openOptionsMenu();
                }
            });
        }

        if (!HardwareUtils.isTablet()) {
            // Set a touch delegate to Tabs button, so the touch events on its tail
            // are passed to the menu button.
            mLayout.post(new Runnable() {
                @Override
                public void run() {
                    int height = mTabs.getHeight();
                    int width = mTabs.getWidth();
                    int tail = (width - height) / 2;

                    Rect bounds = new Rect(0, 0, tail, height);
                    TailTouchDelegate delegate = new TailTouchDelegate(bounds, mShadow);
                    mTabs.setTouchDelegate(delegate);
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
                        @Override
                        public void onDismiss() {
                            mActivity.onOptionsMenuClosed(null);
                        }
                    });
                }
            }
        }

        mFocusOrder = Arrays.asList(mBack, mForward, mLayout, mReader, mSiteSecurity, mStop, mTabs);
    }

    public View getLayout() {
        return mLayout;
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch(msg) {
            case TITLE:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    updateTitle();
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
                    // Reset the title in case we haven't navigated to a new page yet.
                    updateTitle();
                }
                break;
            case RESTORED:
                // TabCount fixup after OOM
            case SELECTED:
                updateTabCount(Tabs.getInstance().getDisplayCount());
                mSwitchingTabs = true;
                // fall through
            case LOCATION_CHANGE:
            case LOAD_ERROR:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    refresh();
                }
                mSwitchingTabs = false;
                break;
            case CLOSED:
            case ADDED:
                updateTabCount(Tabs.getInstance().getDisplayCount());
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    updateBackButton(tab.canDoBack());
                    updateForwardButton(tab.canDoForward());
                }
                break;
            case FAVICON:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    setFavicon(tab.getFavicon());
                }
                break;
            case SECURITY_CHANGE:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    setSecurityMode(tab.getSecurityMode());
                }
                break;
            case READER_ENABLED:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    setReaderMode(tab.getReaderEnabled());
                }
                break;
        }
    }

    public boolean isVisible() {
        return mLayout.getScrollY() == 0;
    }

    public void setNextFocusDownId(int nextId) {
        mLayout.setNextFocusDownId(nextId);
        mTabs.setNextFocusDownId(nextId);
        mBack.setNextFocusDownId(nextId);
        mForward.setNextFocusDownId(nextId);
        mFavicon.setNextFocusDownId(nextId);
        mStop.setNextFocusDownId(nextId);
        mSiteSecurity.setNextFocusDownId(nextId);
        mReader.setNextFocusDownId(nextId);
        mMenu.setNextFocusDownId(nextId);
    }

    @Override
    public void onAnimationStart(Animation animation) {
        if (animation.equals(mLockFadeIn)) {
            if (mSiteSecurityVisible)
                mSiteSecurity.setVisibility(View.VISIBLE);
        } else if (animation.equals(mTitleSlideLeft)) {
            // These two animations may be scheduled to start while the forward
            // animation is occurring. If we're showing the site security icon, make
            // sure it doesn't take any space during the forward transition.
            mSiteSecurity.setVisibility(View.GONE);
        } else if (animation.equals(mTitleSlideRight)) {
            // If we're hiding the icon, make sure that we keep its padding
            // in place during the forward transition
            mSiteSecurity.setVisibility(View.INVISIBLE);
        }
    }

    @Override
    public void onAnimationRepeat(Animation animation) {
    }

    @Override
    public void onAnimationEnd(Animation animation) {
        if (animation.equals(mTitleSlideRight)) {
            mSiteSecurity.startAnimation(mLockFadeIn);
        }
    }

    private int getAwesomeBarEntryTranslation() {
        return mLayout.getWidth() - mAwesomeBarEntry.getRight();
    }

    private int getAwesomeBarCurveTranslation() {
        return mLayout.getWidth() - mTabs.getLeft();
    }

    public void fromAwesomeBarSearch(String url) {
        // Update the title with the url that was just entered. Don't update the title if
        // the AwesomeBar activity was cancelled, or if the user entered an empty string.
        if (url != null && url.length() > 0) {
            setTitle(url);
        }

        if (HardwareUtils.isTablet() || Build.VERSION.SDK_INT < 11) {
            return;
        }

        // If the awesomebar entry is not selected at this point, this means that
        // we had to reinflate the toolbar layout for some reason (device rotation
        // while in awesome screen, activity was killed in background, etc). In this
        // case, we have to ensure the toolbar is in the correct initial state to
        // shrink back.
        if (!mLayout.isSelected()) {
            // Keep the entry highlighted during the animation
            mLayout.setSelected(true);

            final int entryTranslation = getAwesomeBarEntryTranslation();
            final int curveTranslation = getAwesomeBarCurveTranslation();

            if (mAwesomeBarRightEdge != null) {
                ViewHelper.setTranslationX(mAwesomeBarRightEdge, entryTranslation);
            }

            ViewHelper.setTranslationX(mTabs, curveTranslation);
            ViewHelper.setTranslationX(mTabsCounter, curveTranslation);
            ViewHelper.setTranslationX(mActionItemBar, curveTranslation);

            if (mHasSoftMenuButton) {
                ViewHelper.setTranslationX(mMenu, curveTranslation);
                ViewHelper.setTranslationX(mMenuIcon, curveTranslation);
            }

            ViewHelper.setAlpha(mReader, 0);
            ViewHelper.setAlpha(mStop, 0);
        }

        final PropertyAnimator contentAnimator = new PropertyAnimator(250);
        contentAnimator.setUseHardwareLayer(false);

        // Shrink the awesome entry back to its original size

        if (mAwesomeBarRightEdge != null) {
            contentAnimator.attach(mAwesomeBarRightEdge,
                                   PropertyAnimator.Property.TRANSLATION_X,
                                   0);
        }

        contentAnimator.attach(mTabs,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(mTabsCounter,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(mActionItemBar,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);

        if (mHasSoftMenuButton) {
            contentAnimator.attach(mMenu,
                                   PropertyAnimator.Property.TRANSLATION_X,
                                   0);
            contentAnimator.attach(mMenuIcon,
                                   PropertyAnimator.Property.TRANSLATION_X,
                                   0);
        }

        contentAnimator.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                // Turn off selected state on the entry
                mLayout.setSelected(false);

                PropertyAnimator buttonsAnimator = new PropertyAnimator(300);

                // Fade toolbar buttons (reader, stop) after the entry
                // is schrunk back to its original size.
                buttonsAnimator.attach(mReader,
                                       PropertyAnimator.Property.ALPHA,
                                       1);
                buttonsAnimator.attach(mStop,
                                       PropertyAnimator.Property.ALPHA,
                                       1);

                buttonsAnimator.start();

                mAnimatingEntry = false;

                // Trigger animation to update the tabs counter once the
                // tabs button is back on screen.
                updateTabCount(Tabs.getInstance().getDisplayCount());
            }
        });

        mAnimatingEntry = true;

        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                contentAnimator.start();
            }
        }, 500);
    }

    private void onAwesomeBarSearch() {
        // This animation doesn't make much sense in a sidebar UI
        if (HardwareUtils.isTablet() || Build.VERSION.SDK_INT < 11) {
            mActivity.onSearchRequested();
            return;
        }

        if (mAnimatingEntry)
            return;

        final PropertyAnimator contentAnimator = new PropertyAnimator(250);
        contentAnimator.setUseHardwareLayer(false);

        final int entryTranslation = getAwesomeBarEntryTranslation();
        final int curveTranslation = getAwesomeBarCurveTranslation();

        // Keep the entry highlighted during the animation
        mLayout.setSelected(true);

        // Hide stop/reader buttons immediately
        ViewHelper.setAlpha(mReader, 0);
        ViewHelper.setAlpha(mStop, 0);

        // Slide the right side elements of the toolbar

        if (mAwesomeBarRightEdge != null) {
            contentAnimator.attach(mAwesomeBarRightEdge,
                                   PropertyAnimator.Property.TRANSLATION_X,
                                   entryTranslation);
        }

        contentAnimator.attach(mTabs,
                               PropertyAnimator.Property.TRANSLATION_X,
                               curveTranslation);
        contentAnimator.attach(mTabsCounter,
                               PropertyAnimator.Property.TRANSLATION_X,
                               curveTranslation);
        contentAnimator.attach(mActionItemBar,
                               PropertyAnimator.Property.TRANSLATION_X,
                               curveTranslation);

        if (mHasSoftMenuButton) {
            contentAnimator.attach(mMenu,
                                   PropertyAnimator.Property.TRANSLATION_X,
                                   curveTranslation);
            contentAnimator.attach(mMenuIcon,
                                   PropertyAnimator.Property.TRANSLATION_X,
                                   curveTranslation);
        }

        contentAnimator.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                // Once the entry is fully expanded, start awesome screen
                mActivity.onSearchRequested();
                mAnimatingEntry = false;
            }
        });

        mAnimatingEntry = true;
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

            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                if (!tab.isPrivate())
                    mActivity.showNormalTabs();
                else
                    mActivity.showPrivateTabs();
            }
        }
    }

    public void updateTabCount(int count) {
        // If toolbar is selected, this means the entry is expanded and the
        // tabs button is translated offscreen. Don't trigger tabs counter
        // updates until the tabs button is back on screen.
        // See fromAwesomeBarSearch()
        if (mLayout.isSelected()) {
            return;
        }

        // Set TabCounter based on visibility
        if (isVisible() && ViewHelper.getAlpha(mTabsCounter) != 0) {
            mTabsCounter.setCountWithAnimation(count);
        } else {
            mTabsCounter.setCount(count);
        }

        // Update A11y information
        mTabs.setContentDescription((count > 1) ?
                                    mActivity.getString(R.string.num_tabs, count) :
                                    mActivity.getString(R.string.one_tab));
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

        // Handle the readerMode image and visibility: We show the reader mode button if 1) you can
        // enter reader mode for current page or 2) if you're already in reader mode,
        // in which case we show the reader mode "close" (reader_active) icon.
        boolean inReaderMode = false;
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null)
            inReaderMode = ReaderModeUtils.isAboutReader(tab.getURL());
        mReader.setImageResource(inReaderMode ? R.drawable.reader_active : R.drawable.reader);

        mReader.setVisibility(!isLoading && (mShowReader || inReaderMode) ? View.VISIBLE : View.GONE);

        // We want title to fill the whole space available for it when there are icons
        // being shown on the right side of the toolbar as the icons already have some
        // padding in them. This is just to avoid wasting space when icons are shown.
        mTitle.setPadding(0, 0, (!isLoading && !(mShowReader || inReaderMode) ? mTitlePadding : 0), 0);

        updateFocusOrder();
    }

    private void setSiteSecurityVisibility(final boolean visible) {
        if (visible == mSiteSecurityVisible)
            return;

        mSiteSecurityVisible = visible;

        if (mSwitchingTabs) {
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

        if (mForwardAnim != null) {
            long delay = mForwardAnim.getRemainingTime();
            mTitleSlideRight.setStartOffset(delay);
            mTitleSlideLeft.setStartOffset(delay);
        } else {
            mTitleSlideRight.setStartOffset(0);
            mTitleSlideLeft.setStartOffset(0);
        }

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

        // Only set shadow to visible when not on about screens except about:blank.
        visible &= !(url == null || (url.startsWith("about:") && 
                     !url.equals("about:blank")));

        if ((mShadow.getVisibility() == View.VISIBLE) != visible) {
            mShadow.setVisibility(visible ? View.VISIBLE : View.GONE);
        }
    }

    private void setTitle(CharSequence title) {
        mTitle.setText(title);
        mLayout.setContentDescription(title != null ? title : mTitle.getHint());
    }

    // Sets the toolbar title according to the selected tab, obeying the mShowUrl prference.
    private void updateTitle() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        // Keep the title unchanged if there's no selected tab, or if the tab is entering reader mode.
        if (tab == null || tab.isEnteringReaderMode()) {
            return;
        }

        String url = tab.getURL();
        // Setting a null title will ensure we just see the "Enter Search or Address" placeholder text.
        if ("about:home".equals(url) || "about:privatebrowsing".equals(url)) {
            setTitle(null);
            return;
        }

        // If the pref to show the URL isn't set, just use the tab's display title.
        if (!mShowUrl || url == null) {
            setTitle(tab.getDisplayTitle());
            return;
        }

        url = StringUtils.stripScheme(url);
        CharSequence title = StringUtils.stripCommonSubdomains(url);

        String baseDomain = tab.getBaseDomain();
        if (!TextUtils.isEmpty(baseDomain)) {
            SpannableStringBuilder builder = new SpannableStringBuilder(title);
            int index = title.toString().indexOf(baseDomain);
            if (index > -1) {
                builder.setSpan(mUrlColor, 0, title.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);
                builder.setSpan(tab.isPrivate() ? mPrivateDomainColor : mDomainColor, index, index+baseDomain.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);
                title = builder;
            }
        }

        setTitle(title);
    }

    private void setFavicon(Bitmap image) {
        if (Tabs.getInstance().getSelectedTab().getState() == Tab.STATE_LOADING)
            return;

        if (image != null) {
            image = Bitmap.createScaledBitmap(image, mFaviconSize, mFaviconSize, false);
            mFavicon.setImageBitmap(image);
        } else {
            mFavicon.setImageResource(R.drawable.favicon);
        }
    }
    
    private void setSecurityMode(String mode) {
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

    private void setReaderMode(boolean showReader) {
        mShowReader = showReader;
        setPageActionVisibility(mStop.getVisibility() == View.VISIBLE);
    }

    public void requestFocusFromTouch() {
        mLayout.requestFocusFromTouch();
    }

    public void prepareTabsAnimation(PropertyAnimator animator, boolean tabsAreShown) {
        if (!tabsAreShown) {
            PropertyAnimator buttonsAnimator =
                    new PropertyAnimator(animator.getDuration(), sButtonsInterpolator);

            buttonsAnimator.attach(mTabsCounter,
                                   PropertyAnimator.Property.ALPHA,
                                   1.0f);

            if (mHasSoftMenuButton && !HardwareUtils.isTablet()) {
                buttonsAnimator.attach(mMenuIcon,
                                       PropertyAnimator.Property.ALPHA,
                                       1.0f);
            }

            buttonsAnimator.start();

            return;
        }

        ViewHelper.setAlpha(mTabsCounter, 0.0f);

        if (mHasSoftMenuButton && !HardwareUtils.isTablet()) {
            ViewHelper.setAlpha(mMenuIcon, 0.0f);
        }
    }

    public void updateBackButton(boolean enabled) {
         Drawable drawable = mBack.getDrawable();
         if (drawable != null)
             drawable.setAlpha(enabled ? 255 : 77);

         mBack.setEnabled(enabled);
    }

    public void updateForwardButton(final boolean enabled) {
        if (mForward.isEnabled() == enabled)
            return;

        // Save the state on the forward button so that we can skip animations
        // when there's nothing to change
        mForward.setEnabled(enabled);

        if (mForward.getVisibility() != View.VISIBLE)
            return;

        // We want the forward button to show immediately when switching tabs
        mForwardAnim = new PropertyAnimator(mSwitchingTabs ? 10 : FORWARD_ANIMATION_DURATION);
        final int width = mForward.getWidth() / 2;

        mForwardAnim.setPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                if (!enabled) {
                    // Set the margin before the transition when hiding the forward button. We
                    // have to do this so that the favicon isn't clipped during the transition
                    ViewGroup.MarginLayoutParams layoutParams =
                        (ViewGroup.MarginLayoutParams)mUrlDisplayContainer.getLayoutParams();
                    layoutParams.leftMargin = 0;
                    mUrlDisplayContainer.requestLayout();
                    // Note, we already translated the favicon, site security, and text field
                    // in prepareForwardAnimation, so they should appear to have not moved at
                    // all at this point.
                }
            }

            @Override
            public void onPropertyAnimationEnd() {
                if (enabled) {
                    ViewGroup.MarginLayoutParams layoutParams =
                        (ViewGroup.MarginLayoutParams)mUrlDisplayContainer.getLayoutParams();
                    layoutParams.leftMargin = mAddressBarViewOffset;

                    ViewHelper.setTranslationX(mTitle, 0);
                    ViewHelper.setTranslationX(mFavicon, 0);
                    ViewHelper.setTranslationX(mSiteSecurity, 0);
                }

                ViewGroup.MarginLayoutParams layoutParams =
                    (ViewGroup.MarginLayoutParams)mForward.getLayoutParams();
                layoutParams.leftMargin = mDefaultForwardMargin + (mForward.isEnabled() ? width : 0);
                ViewHelper.setTranslationX(mForward, 0);

                mUrlDisplayContainer.requestLayout();
                mForwardAnim = null;
            }
        });

        prepareForwardAnimation(mForwardAnim, enabled, width);
        mForwardAnim.start();
    }

    private void prepareForwardAnimation(PropertyAnimator anim, boolean enabled, int width) {
        if (!enabled) {
            anim.attach(mForward,
                      PropertyAnimator.Property.TRANSLATION_X,
                      -width);
            anim.attach(mForward,
                      PropertyAnimator.Property.ALPHA,
                      0);
            anim.attach(mTitle,
                      PropertyAnimator.Property.TRANSLATION_X,
                      0);
            anim.attach(mFavicon,
                      PropertyAnimator.Property.TRANSLATION_X,
                      0);
            anim.attach(mSiteSecurity,
                      PropertyAnimator.Property.TRANSLATION_X,
                      0);

            // We're hiding the forward button. We're going to reset the margin before
            // the animation starts, so we shift these items to the right so that they don't
            // appear to move initially.
            ViewHelper.setTranslationX(mTitle, mAddressBarViewOffset);
            ViewHelper.setTranslationX(mFavicon, mAddressBarViewOffset);
            ViewHelper.setTranslationX(mSiteSecurity, mAddressBarViewOffset);
        } else {
            anim.attach(mForward,
                      PropertyAnimator.Property.TRANSLATION_X,
                      width);
            anim.attach(mForward,
                      PropertyAnimator.Property.ALPHA,
                      1);
            anim.attach(mTitle,
                      PropertyAnimator.Property.TRANSLATION_X,
                      mAddressBarViewOffset);
            anim.attach(mFavicon,
                      PropertyAnimator.Property.TRANSLATION_X,
                      mAddressBarViewOffset);
            anim.attach(mSiteSecurity,
                      PropertyAnimator.Property.TRANSLATION_X,
                      mAddressBarViewOffset);
        }
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
            updateTitle();
            setFavicon(tab.getFavicon());
            setProgressVisibility(tab.getState() == Tab.STATE_LOADING);
            setSecurityMode(tab.getSecurityMode());
            setReaderMode(tab.getReaderEnabled());
            setShadowVisibility(true);
            updateBackButton(tab.canDoBack());
            updateForwardButton(tab.canDoForward());

            final boolean isPrivate = tab.isPrivate();
            mAddressBarBg.setPrivateMode(isPrivate);
            mLayout.setPrivateMode(isPrivate);
            mTabs.setPrivateMode(isPrivate);
            mTitle.setPrivateMode(isPrivate);
            mMenu.setPrivateMode(isPrivate);
            mMenuIcon.setPrivateMode(isPrivate);

            if (mBack instanceof BackButton)
                ((BackButton) mBack).setPrivateMode(isPrivate);

            if (mForward instanceof ForwardButton)
                ((ForwardButton) mForward).setPrivateMode(isPrivate);
        }
    }

    public void onDestroy() {
        if (mPrefObserverId != null) {
             PrefsHelper.removeObserver(mPrefObserverId);
             mPrefObserverId = null;
        }
        Tabs.unregisterOnTabsChangedListener(this);
    }

    public boolean openOptionsMenu() {
        if (!mHasSoftMenuButton)
            return false;

        GeckoAppShell.getGeckoInterface().invalidateOptionsMenu();
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
}
