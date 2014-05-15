/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.List;

import org.json.JSONObject;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.LightweightTheme;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.PropertyAnimator.PropertyAnimationListener;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.MenuPopup;
import org.mozilla.gecko.toolbar.ToolbarDisplayLayout.OnStopListener;
import org.mozilla.gecko.toolbar.ToolbarDisplayLayout.OnTitleChangeListener;
import org.mozilla.gecko.toolbar.ToolbarDisplayLayout.UpdateFlags;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.MenuUtils;
import org.mozilla.gecko.widget.GeckoImageButton;
import org.mozilla.gecko.widget.GeckoImageView;
import org.mozilla.gecko.widget.GeckoRelativeLayout;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.Interpolator;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;

/**
* {@code BrowserToolbar} is single entry point for users of the toolbar
* subsystem i.e. this should be the only import outside the 'toolbar'
* package.
*
* {@code BrowserToolbar} serves at the single event bus for all
* sub-components in the toolbar. It tracks tab events and gecko messages
* and update the state of its inner components accordingly.
*
* It has two states, display and edit, which are controlled by
* ToolbarEditLayout and ToolbarDisplayLayout. In display state, the toolbar
* displays the current state for the selected tab. In edit state, it shows
* a text entry for searching bookmarks/history. {@code BrowserToolbar}
* provides public API to enter, cancel, and commit the edit state as well
* as a set of listeners to allow {@code BrowserToolbar} users to react
* to state changes accordingly.
*/
public class BrowserToolbar extends GeckoRelativeLayout
                            implements Tabs.OnTabsChangedListener,
                                       GeckoMenu.ActionItemBarPresenter,
                                       GeckoEventListener {
    private static final String LOGTAG = "GeckoToolbar";

    public interface OnActivateListener {
        public void onActivate();
    }

    public interface OnCommitListener {
        public void onCommit();
    }

    public interface OnDismissListener {
        public void onDismiss();
    }

    public interface OnFilterListener {
        public void onFilter(String searchText, AutocompleteHandler handler);
    }

    public interface OnStartEditingListener {
        public void onStartEditing();
    }

    public interface OnStopEditingListener {
        public void onStopEditing();
    }

    private enum UIMode {
        EDIT,
        DISPLAY
    }

    enum ForwardButtonAnimation {
        SHOW,
        HIDE
    }

    private ToolbarDisplayLayout mUrlDisplayLayout;
    private ToolbarEditLayout mUrlEditLayout;
    private View mUrlBarEntry;
    private ImageView mUrlBarRightEdge;
    private boolean mSwitchingTabs;
    private ShapedButton mTabs;
    private ImageButton mBack;
    private ImageButton mForward;

    private ToolbarProgressView mProgressBar;
    private TabCounter mTabsCounter;
    private GeckoImageButton mMenu;
    private GeckoImageView mMenuIcon;
    private LinearLayout mActionItemBar;
    private MenuPopup mMenuPopup;
    private List<View> mFocusOrder;

    private OnActivateListener mActivateListener;
    private OnCommitListener mCommitListener;
    private OnDismissListener mDismissListener;
    private OnFilterListener mFilterListener;
    private OnFocusChangeListener mFocusChangeListener;
    private OnStartEditingListener mStartEditingListener;
    private OnStopEditingListener mStopEditingListener;

    final private BrowserApp mActivity;
    private boolean mHasSoftMenuButton;

    private UIMode mUIMode;
    private boolean mAnimatingEntry;

    private int mUrlBarViewOffset;
    private int mDefaultForwardMargin;

    private static final Interpolator sButtonsInterpolator = new AccelerateInterpolator();

    private static final int FORWARD_ANIMATION_DURATION = 450;

    private final LightweightTheme mTheme;

    public BrowserToolbar(Context context) {
        this(context, null);
    }

    public BrowserToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mTheme = ((GeckoApplication) context.getApplicationContext()).getLightweightTheme();

        // BrowserToolbar is attached to BrowserApp only.
        mActivity = (BrowserApp) context;

        // Inflate the content.
        LayoutInflater.from(context).inflate(R.layout.browser_toolbar, this);

        Tabs.registerOnTabsChangedListener(this);
        mSwitchingTabs = true;
        mAnimatingEntry = false;

        registerEventListener("Reader:Click");
        registerEventListener("Reader:LongClick");

        mAnimatingEntry = false;

        final Resources res = getResources();
        mUrlBarViewOffset = res.getDimensionPixelSize(R.dimen.url_bar_offset_left);
        mDefaultForwardMargin = res.getDimensionPixelSize(R.dimen.forward_default_offset);
        mUrlDisplayLayout = (ToolbarDisplayLayout) findViewById(R.id.display_layout);
        mUrlBarEntry = findViewById(R.id.url_bar_entry);
        mUrlEditLayout = (ToolbarEditLayout) findViewById(R.id.edit_layout);

        // This will clip the right edge's image at 60% of its width
        mUrlBarRightEdge = (ImageView) findViewById(R.id.url_bar_right_edge);
        if (mUrlBarRightEdge != null) {
            mUrlBarRightEdge.getDrawable().setLevel(6000);
        }

        mTabs = (ShapedButton) findViewById(R.id.tabs);
        mTabsCounter = (TabCounter) findViewById(R.id.tabs_counter);
        if (Build.VERSION.SDK_INT >= 11) {
            mTabsCounter.setLayerType(View.LAYER_TYPE_SOFTWARE, null);
        }

        mBack = (ImageButton) findViewById(R.id.back);
        setButtonEnabled(mBack, false);
        mForward = (ImageButton) findViewById(R.id.forward);
        setButtonEnabled(mForward, false);

        mMenu = (GeckoImageButton) findViewById(R.id.menu);
        mMenuIcon = (GeckoImageView) findViewById(R.id.menu_icon);
        mActionItemBar = (LinearLayout) findViewById(R.id.menu_items);
        mHasSoftMenuButton = !HardwareUtils.hasMenuButton();

        // We use different layouts on phones and tablets, so adjust the focus
        // order appropriately.
        mFocusOrder = new ArrayList<View>();
        if (HardwareUtils.isTablet()) {
            mFocusOrder.addAll(Arrays.asList(mTabs, mBack, mForward, this));
            mFocusOrder.addAll(mUrlDisplayLayout.getFocusOrder());
            mFocusOrder.addAll(Arrays.asList(mActionItemBar, mMenu));
        } else {
            mFocusOrder.add(this);
            mFocusOrder.addAll(mUrlDisplayLayout.getFocusOrder());
            mFocusOrder.addAll(Arrays.asList(mTabs, mMenu));
        }

        setUIMode(UIMode.DISPLAY);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mActivateListener != null) {
                    mActivateListener.onActivate();
                }
            }
        });

        setOnCreateContextMenuListener(new View.OnCreateContextMenuListener() {
            @Override
            public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
                // We don't the context menu while editing
                if (isEditing()) {
                    return;
                }

                // NOTE: Use MenuUtils.safeSetVisible because some actions might
                // be on the Page menu

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
                        menu.findItem(R.id.add_to_launcher).setVisible(false);
                        MenuUtils.safeSetVisible(menu, R.id.share, false);
                    }

                    MenuUtils.safeSetVisible(menu, R.id.subscribe, tab.hasFeeds());
                    MenuUtils.safeSetVisible(menu, R.id.add_search_engine, tab.hasOpenSearch());
                } else {
                    // if there is no tab, remove anything tab dependent
                    menu.findItem(R.id.copyurl).setVisible(false);
                    menu.findItem(R.id.add_to_launcher).setVisible(false);
                    MenuUtils.safeSetVisible(menu, R.id.share, false);
                    MenuUtils.safeSetVisible(menu, R.id.subscribe, false);
                    MenuUtils.safeSetVisible(menu, R.id.add_search_engine, false);
                }

                MenuUtils.safeSetVisible(menu, R.id.share, !GeckoProfile.get(getContext()).inGuestMode());
            }
        });

        mUrlDisplayLayout.setOnStopListener(new OnStopListener() {
            @Override
            public Tab onStop() {
                final Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab != null) {
                    tab.doStop();
                    return tab;
                }

                return null;
            }
        });

        mUrlDisplayLayout.setOnTitleChangeListener(new OnTitleChangeListener() {
            @Override
            public void onTitleChange(CharSequence title) {
                final String contentDescription;
                if (title != null) {
                    contentDescription = title.toString();
                } else {
                    contentDescription = mActivity.getString(R.string.url_bar_default_text);
                }

                // The title and content description should
                // always be sync.
                setContentDescription(contentDescription);
            }
        });

        mUrlEditLayout.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                setSelected(hasFocus);
                if (mFocusChangeListener != null) {
                    mFocusChangeListener.onFocusChange(v, hasFocus);
                }
            }
        });

        mTabs.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleTabs();
            }
        });
        mTabs.setImageLevel(0);

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
    }

    public void setProgressBar(ToolbarProgressView progressBar) {
        mProgressBar = progressBar;
    }

    public void refresh() {
        mUrlDisplayLayout.dismissSiteIdentityPopup();
    }

    public boolean onBackPressed() {
        if (isEditing()) {
            stopEditing();
            return true;
        }

        return mUrlDisplayLayout.dismissSiteIdentityPopup();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // If the motion event has occured below the toolbar (due to the scroll
        // offset), let it pass through to the page.
        if (event != null && event.getY() > getHeight() + ViewHelper.getTranslationY(this)) {
            return false;
        }

        return super.onTouchEvent(event);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        if (h != oldh) {
            // Post this to happen outside of onSizeChanged, as this may cause
            // a layout change and relayouts within a layout change don't work.
            post(new Runnable() {
                @Override
                public void run() {
                    mActivity.refreshToolbarHeight();
                }
            });
        }
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        Log.d(LOGTAG, "onTabChanged: " + msg);
        final Tabs tabs = Tabs.getInstance();

        // These conditions are split into three phases:
        // * Always do first
        // * Handling specific to the selected tab
        // * Always do afterwards.

        switch (msg) {
            case ADDED:
            case CLOSED:
                updateTabCount(tabs.getDisplayCount());
                break;
            case RESTORED:
                // TabCount fixup after OOM
            case SELECTED:
                mUrlDisplayLayout.dismissSiteIdentityPopup();
                updateTabCount(tabs.getDisplayCount());
                mSwitchingTabs = true;
                break;
        }

        if (tabs.isSelectedTab(tab)) {
            final EnumSet<UpdateFlags> flags = EnumSet.noneOf(UpdateFlags.class);

            // Progress-related handling
            switch (msg) {
                case START:
                    updateProgressVisibility(tab, Tab.LOAD_PROGRESS_INIT);
                    // Fall through.
                case ADDED:
                case LOCATION_CHANGE:
                case LOAD_ERROR:
                case LOADED:
                case STOP:
                    flags.add(UpdateFlags.PROGRESS);
                    if (mProgressBar.getVisibility() == View.VISIBLE) {
                        mProgressBar.animateProgress(tab.getLoadProgress());
                    }
                    break;

                case SELECTED:
                    flags.add(UpdateFlags.PROGRESS);
                    updateProgressVisibility();
                    break;
            }

            switch (msg) {
                case STOP:
                    // Reset the title in case we haven't navigated
                    // to a new page yet.
                    flags.add(UpdateFlags.TITLE);
                    // Fall through.
                case START:
                case CLOSED:
                case ADDED:
                    updateBackButton(tab);
                    updateForwardButton(tab);
                    break;

                case SELECTED:
                    flags.add(UpdateFlags.PRIVATE_MODE);
                    setPrivateMode(tab.isPrivate());
                    // Fall through.
                case LOAD_ERROR:
                    flags.add(UpdateFlags.TITLE);
                    // Fall through.
                case LOCATION_CHANGE:
                    // A successful location change will cause Tab to notify
                    // us of a title change, so we don't update the title here.
                    flags.add(UpdateFlags.FAVICON);
                    flags.add(UpdateFlags.SITE_IDENTITY);

                    updateBackButton(tab);
                    updateForwardButton(tab);
                    break;

                case TITLE:
                    flags.add(UpdateFlags.TITLE);
                    break;

                case FAVICON:
                    flags.add(UpdateFlags.FAVICON);
                    break;

                case SECURITY_CHANGE:
                    flags.add(UpdateFlags.SITE_IDENTITY);
                    break;
            }

            if (!flags.isEmpty()) {
                updateDisplayLayout(tab, flags);
            }
        }

        switch (msg) {
            case SELECTED:
            case LOAD_ERROR:
            case LOCATION_CHANGE:
                mSwitchingTabs = false;
        }
    }

    private void updateProgressVisibility() {
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        updateProgressVisibility(selectedTab, selectedTab.getLoadProgress());
    }

    private void updateProgressVisibility(Tab selectedTab, int progress) {
        if (!isEditing() && selectedTab.getState() == Tab.STATE_LOADING) {
            mProgressBar.setProgress(progress);
            mProgressBar.setVisibility(View.VISIBLE);
        } else {
            mProgressBar.setVisibility(View.GONE);
        }
    }

    public boolean isVisible() {
        return ViewHelper.getTranslationY(this) == 0;
    }

    @Override
    public void setNextFocusDownId(int nextId) {
        super.setNextFocusDownId(nextId);
        mTabs.setNextFocusDownId(nextId);
        mBack.setNextFocusDownId(nextId);
        mForward.setNextFocusDownId(nextId);
        mUrlDisplayLayout.setNextFocusDownId(nextId);
        mMenu.setNextFocusDownId(nextId);
    }

    private int getUrlBarEntryTranslation() {
        return getWidth() - mUrlBarEntry.getRight();
    }

    private int getUrlBarCurveTranslation() {
        return getWidth() - mTabs.getLeft();
    }

    private boolean canDoBack(Tab tab) {
        return (tab.canDoBack() && !isEditing());
    }

    private boolean canDoForward(Tab tab) {
        return (tab.canDoForward() && !isEditing());
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

    private void updateTabCountAndAnimate(int count) {
        // Don't animate if the toolbar is hidden.
        if (!isVisible()) {
            updateTabCount(count);
            return;
        }

        // If toolbar is in edit mode on a phone, this means the entry is expanded
        // and the tabs button is translated offscreen. Don't trigger tabs counter
        // updates until the tabs button is back on screen.
        // See stopEditing()
        if (!isEditing() || HardwareUtils.isTablet()) {
            mTabsCounter.setCount(count);

            mTabs.setContentDescription((count > 1) ?
                                        mActivity.getString(R.string.num_tabs, count) :
                                        mActivity.getString(R.string.one_tab));
        }
    }

    private void updateTabCount(int count) {
        // If toolbar is in edit mode on a phone, this means the entry is expanded
        // and the tabs button is translated offscreen. Don't trigger tabs counter
        // updates until the tabs button is back on screen.
        // See stopEditing()
        if (isEditing() && !HardwareUtils.isTablet()) {
            return;
        }

        // Set TabCounter based on visibility
        if (isVisible() && ViewHelper.getAlpha(mTabsCounter) != 0 && !isEditing()) {
            mTabsCounter.setCountWithAnimation(count);
        } else {
            mTabsCounter.setCount(count);
        }

        // Update A11y information
        mTabs.setContentDescription((count > 1) ?
                                    mActivity.getString(R.string.num_tabs, count) :
                                    mActivity.getString(R.string.one_tab));
    }

    private void updateDisplayLayout(Tab tab, EnumSet<UpdateFlags> flags) {
        if (mSwitchingTabs) {
            flags.add(UpdateFlags.DISABLE_ANIMATIONS);
        }

        mUrlDisplayLayout.updateFromTab(tab, flags);

        if (flags.contains(UpdateFlags.TITLE)) {
            if (!isEditing()) {
                mUrlEditLayout.setText(tab.getURL());
            }
        }

        if (flags.contains(UpdateFlags.PROGRESS)) {
            updateFocusOrder();
        }
    }

    private void updateFocusOrder() {
        View prevView = null;

        // If the element that has focus becomes disabled or invisible, focus
        // is given to the URL bar.
        boolean needsNewFocus = false;

        for (View view : mFocusOrder) {
            if (view.getVisibility() != View.VISIBLE || !view.isEnabled()) {
                if (view.hasFocus()) {
                    needsNewFocus = true;
                }
                continue;
            }

            if (view == mActionItemBar) {
                final int childCount = mActionItemBar.getChildCount();
                for (int child = 0; child < childCount; child++) {
                    View childView = mActionItemBar.getChildAt(child);
                    if (prevView != null) {
                        childView.setNextFocusLeftId(prevView.getId());
                        prevView.setNextFocusRightId(childView.getId());
                    }
                    prevView = childView;
                }
            } else {
                if (prevView != null) {
                    view.setNextFocusLeftId(prevView.getId());
                    prevView.setNextFocusRightId(view.getId());
                }
                prevView = view;
            }
        }

        if (needsNewFocus) {
            requestFocus();
        }
    }

    public void onEditSuggestion(String suggestion) {
        if (!isEditing()) {
            return;
        }

        mUrlEditLayout.onEditSuggestion(suggestion);
    }

    public void setTitle(CharSequence title) {
        mUrlDisplayLayout.setTitle(title);
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

    public void finishTabsAnimation(boolean tabsAreShown) {
        if (tabsAreShown) {
            return;
        }

        PropertyAnimator animator = new PropertyAnimator(150);

        animator.attach(mTabsCounter,
                        PropertyAnimator.Property.ALPHA,
                        1.0f);

        if (mHasSoftMenuButton && !HardwareUtils.isTablet()) {
            animator.attach(mMenuIcon,
                            PropertyAnimator.Property.ALPHA,
                            1.0f);
        }

        animator.start();
    }

    public void setOnActivateListener(OnActivateListener listener) {
        mActivateListener = listener;
    }

    public void setOnCommitListener(OnCommitListener listener) {
        mCommitListener = listener;
        mUrlEditLayout.setOnCommitListener(listener);
    }

    public void setOnDismissListener(OnDismissListener listener) {
        mDismissListener = listener;
        mUrlEditLayout.setOnDismissListener(listener);
    }

    public void setOnFilterListener(OnFilterListener listener) {
        mFilterListener = listener;
        mUrlEditLayout.setOnFilterListener(listener);
    }

    public void setOnFocusChangeListener(OnFocusChangeListener listener) {
        mFocusChangeListener = listener;
    }

    public void setOnStartEditingListener(OnStartEditingListener listener) {
        mStartEditingListener = listener;
    }

    public void setOnStopEditingListener(OnStopEditingListener listener) {
        mStopEditingListener = listener;
    }

    private void showUrlEditLayout() {
        setUrlEditLayoutVisibility(true, null);
    }

    private void showUrlEditLayout(PropertyAnimator animator) {
        setUrlEditLayoutVisibility(true, animator);
    }

    private void hideUrlEditLayout() {
        setUrlEditLayoutVisibility(false, null);
    }

    private void hideUrlEditLayout(PropertyAnimator animator) {
        setUrlEditLayoutVisibility(false, animator);
    }

    private void setUrlEditLayoutVisibility(final boolean showEditLayout, PropertyAnimator animator) {
        final View viewToShow = (showEditLayout ? mUrlEditLayout : mUrlDisplayLayout);
        final View viewToHide = (showEditLayout ? mUrlDisplayLayout : mUrlEditLayout);

        if (showEditLayout) {
            mUrlEditLayout.prepareShowAnimation(animator);
        }

        if (animator == null) {
            viewToHide.setVisibility(View.GONE);
            viewToShow.setVisibility(View.VISIBLE);
            return;
        }

        ViewHelper.setAlpha(viewToShow, 0.0f);
        animator.attach(viewToShow,
                        PropertyAnimator.Property.ALPHA,
                        1.0f);

        animator.attach(viewToHide,
                        PropertyAnimator.Property.ALPHA,
                        0.0f);

        animator.addPropertyAnimationListener(new PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                viewToShow.setVisibility(View.VISIBLE);
            }

            @Override
            public void onPropertyAnimationEnd() {
                viewToHide.setVisibility(View.GONE);
                ViewHelper.setAlpha(viewToHide, 1.0f);
            }
        });
    }

    /**
     * Disables and dims all toolbar elements which are not
     * related to editing mode.
     */
    private void updateChildrenForEditing() {
        // This is for the tablet UI only
        if (!HardwareUtils.isTablet()) {
            return;
        }

        // Disable toolbar elemens while in editing mode
        final boolean enabled = !isEditing();

        // This alpha value has to be in sync with the one used
        // in setButtonEnabled().
        final float alpha = (enabled ? 1.0f : 0.24f);

        if (!enabled) {
            mTabsCounter.onEnterEditingMode();
        }

        mTabs.setEnabled(enabled);
        ViewHelper.setAlpha(mTabsCounter, alpha);
        mMenu.setEnabled(enabled);
        ViewHelper.setAlpha(mMenuIcon, alpha);

        final int actionItemsCount = mActionItemBar.getChildCount();
        for (int i = 0; i < actionItemsCount; i++) {
            mActionItemBar.getChildAt(i).setEnabled(enabled);
        }
        ViewHelper.setAlpha(mActionItemBar, alpha);

        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            setButtonEnabled(mBack, canDoBack(tab));
            setButtonEnabled(mForward, canDoForward(tab));

            // Once the editing mode is finished, we have to ensure that the
            // forward button slides away if necessary. This is because we might
            // have only disabled it (without hiding it) when the toolbar entered
            // editing mode.
            if (!isEditing()) {
                animateForwardButton(canDoForward(tab) ?
                                     ForwardButtonAnimation.SHOW : ForwardButtonAnimation.HIDE);
            }
        }
    }

    private void setUIMode(final UIMode uiMode) {
        mUIMode = uiMode;
        mUrlEditLayout.setEnabled(uiMode == UIMode.EDIT);
    }

    /**
     * Returns whether or not the URL bar is in editing mode (url bar is expanded, hiding the new
     * tab button). Note that selection state is independent of editing mode.
     */
    public boolean isEditing() {
        return (mUIMode == UIMode.EDIT);
    }

    public void startEditing(String url, PropertyAnimator animator) {
        if (isEditing()) {
            return;
        }

        mUrlEditLayout.setText(url != null ? url : "");

        setUIMode(UIMode.EDIT);
        updateChildrenForEditing();

        updateProgressVisibility();

        if (mStartEditingListener != null) {
            mStartEditingListener.onStartEditing();
        }

        if (mUrlBarRightEdge != null) {
            mUrlBarRightEdge.setVisibility(View.VISIBLE);
        }

        final int entryTranslation = getUrlBarEntryTranslation();
        final int curveTranslation = getUrlBarCurveTranslation();

        // This animation doesn't make much sense in a sidebar UI
        if (HardwareUtils.isTablet() || Build.VERSION.SDK_INT < 11) {
            showUrlEditLayout();

            if (!HardwareUtils.isTablet()) {
                if (mUrlBarRightEdge != null) {
                    ViewHelper.setTranslationX(mUrlBarRightEdge, entryTranslation);
                }

                ViewHelper.setTranslationX(mTabs, curveTranslation);
                ViewHelper.setTranslationX(mTabsCounter, curveTranslation);
                ViewHelper.setTranslationX(mActionItemBar, curveTranslation);

                if (mHasSoftMenuButton) {
                    ViewHelper.setTranslationX(mMenu, curveTranslation);
                    ViewHelper.setTranslationX(mMenuIcon, curveTranslation);
                }
            }

            return;
        }

        if (mAnimatingEntry)
            return;

        // Highlight the toolbar from the start of the animation.
        setSelected(true);

        mUrlDisplayLayout.prepareStartEditingAnimation();

        // Slide the right side elements of the toolbar

        if (mUrlBarRightEdge != null) {
            animator.attach(mUrlBarRightEdge,
                            PropertyAnimator.Property.TRANSLATION_X,
                            entryTranslation);
        }

        animator.attach(mTabs,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);
        animator.attach(mTabsCounter,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);
        animator.attach(mActionItemBar,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);

        if (mHasSoftMenuButton) {
            animator.attach(mMenu,
                            PropertyAnimator.Property.TRANSLATION_X,
                            curveTranslation);

            animator.attach(mMenuIcon,
                            PropertyAnimator.Property.TRANSLATION_X,
                            curveTranslation);
        }

        showUrlEditLayout(animator);

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                mAnimatingEntry = false;
            }
        });

        mAnimatingEntry = true;
    }

    /**
     * Exits edit mode without updating the toolbar title.
     *
     * @return the url that was entered
     */
    public String cancelEdit() {
        return stopEditing();
    }

    /**
     * Exits edit mode, updating the toolbar title with the url that was just entered.
     *
     * @return the url that was entered
     */
    public String commitEdit() {
        final String url = stopEditing();
        if (!TextUtils.isEmpty(url)) {
            setTitle(url);
        }
        return url;
    }

    private String stopEditing() {
        final String url = mUrlEditLayout.getText();
        if (!isEditing()) {
            return url;
        }
        setUIMode(UIMode.DISPLAY);

        updateChildrenForEditing();

        if (mStopEditingListener != null) {
            mStopEditingListener.onStopEditing();
        }

        updateProgressVisibility();

        if (HardwareUtils.isTablet() || Build.VERSION.SDK_INT < 11) {
            hideUrlEditLayout();

            if (!HardwareUtils.isTablet()) {
                updateTabCountAndAnimate(Tabs.getInstance().getDisplayCount());

                if (mUrlBarRightEdge != null) {
                    ViewHelper.setTranslationX(mUrlBarRightEdge, 0);
                }

                ViewHelper.setTranslationX(mTabs, 0);
                ViewHelper.setTranslationX(mTabsCounter, 0);
                ViewHelper.setTranslationX(mActionItemBar, 0);

                if (mHasSoftMenuButton) {
                    ViewHelper.setTranslationX(mMenu, 0);
                    ViewHelper.setTranslationX(mMenuIcon, 0);
                }
            }

            return url;
        }

        final PropertyAnimator contentAnimator = new PropertyAnimator(250);
        contentAnimator.setUseHardwareLayer(false);

        // Shrink the urlbar entry back to its original size

        if (mUrlBarRightEdge != null) {
            contentAnimator.attach(mUrlBarRightEdge,
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

        hideUrlEditLayout(contentAnimator);

        contentAnimator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                if (mUrlBarRightEdge != null) {
                    mUrlBarRightEdge.setVisibility(View.INVISIBLE);
                }

                PropertyAnimator buttonsAnimator = new PropertyAnimator(300);
                mUrlDisplayLayout.prepareStopEditingAnimation(buttonsAnimator);
                buttonsAnimator.start();

                mAnimatingEntry = false;

                // Trigger animation to update the tabs counter once the
                // tabs button is back on screen.
                updateTabCountAndAnimate(Tabs.getInstance().getDisplayCount());
            }
        });

        mAnimatingEntry = true;
        contentAnimator.start();

        return url;
    }

    public void setButtonEnabled(ImageButton button, boolean enabled) {
        final Drawable drawable = button.getDrawable();
        if (drawable != null) {
            // This alpha value has to be in sync with the one used
            // in updateChildrenForEditing().
            drawable.setAlpha(enabled ? 255 : 61);
        }

        button.setEnabled(enabled);
    }

    public void updateBackButton(Tab tab) {
        setButtonEnabled(mBack, canDoBack(tab));
    }

    private void animateForwardButton(final ForwardButtonAnimation animation) {
        // If the forward button is not visible, we must be
        // in the phone UI.
        if (mForward.getVisibility() != View.VISIBLE) {
            return;
        }

        final boolean showing = (animation == ForwardButtonAnimation.SHOW);

        // if the forward button's margin is non-zero, this means it has already
        // been animated to be visible¸ and vice-versa.
        MarginLayoutParams fwdParams = (MarginLayoutParams) mForward.getLayoutParams();
        if ((fwdParams.leftMargin > mDefaultForwardMargin && showing) ||
            (fwdParams.leftMargin == mDefaultForwardMargin && !showing)) {
            return;
        }

        // We want the forward button to show immediately when switching tabs
        final PropertyAnimator forwardAnim =
                new PropertyAnimator(mSwitchingTabs ? 10 : FORWARD_ANIMATION_DURATION);
        final int width = mForward.getWidth() / 2;

        forwardAnim.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                if (!showing) {
                    // Set the margin before the transition when hiding the forward button. We
                    // have to do this so that the favicon isn't clipped during the transition
                    MarginLayoutParams layoutParams =
                        (MarginLayoutParams) mUrlDisplayLayout.getLayoutParams();
                    layoutParams.leftMargin = 0;

                    // Do the same on the URL edit container
                    layoutParams = (MarginLayoutParams) mUrlEditLayout.getLayoutParams();
                    layoutParams.leftMargin = 0;

                    requestLayout();
                    // Note, we already translated the favicon, site security, and text field
                    // in prepareForwardAnimation, so they should appear to have not moved at
                    // all at this point.
                }
            }

            @Override
            public void onPropertyAnimationEnd() {
                if (showing) {
                    MarginLayoutParams layoutParams =
                        (MarginLayoutParams) mUrlDisplayLayout.getLayoutParams();
                    layoutParams.leftMargin = mUrlBarViewOffset;

                    layoutParams = (MarginLayoutParams) mUrlEditLayout.getLayoutParams();
                    layoutParams.leftMargin = mUrlBarViewOffset;
                }

                mUrlDisplayLayout.finishForwardAnimation();

                MarginLayoutParams layoutParams = (MarginLayoutParams) mForward.getLayoutParams();
                layoutParams.leftMargin = mDefaultForwardMargin + (showing ? width : 0);
                ViewHelper.setTranslationX(mForward, 0);

                requestLayout();
            }
        });

        prepareForwardAnimation(forwardAnim, animation, width);
        forwardAnim.start();
    }

    public void updateForwardButton(Tab tab) {
        final boolean enabled = canDoForward(tab);
        if (mForward.isEnabled() == enabled)
            return;

        // Save the state on the forward button so that we can skip animations
        // when there's nothing to change
        setButtonEnabled(mForward, enabled);
        animateForwardButton(enabled ? ForwardButtonAnimation.SHOW : ForwardButtonAnimation.HIDE);
    }

    private void prepareForwardAnimation(PropertyAnimator anim, ForwardButtonAnimation animation, int width) {
        if (animation == ForwardButtonAnimation.HIDE) {
            anim.attach(mForward,
                      PropertyAnimator.Property.TRANSLATION_X,
                      -width);
            anim.attach(mForward,
                      PropertyAnimator.Property.ALPHA,
                      0);

        } else {
            anim.attach(mForward,
                      PropertyAnimator.Property.TRANSLATION_X,
                      width);
            anim.attach(mForward,
                      PropertyAnimator.Property.ALPHA,
                      1);
        }

        mUrlDisplayLayout.prepareForwardAnimation(anim, animation, width);
    }

    @Override
    public boolean addActionItem(View actionItem) {
        mActionItemBar.addView(actionItem);
        return true;
    }

    @Override
    public void removeActionItem(View actionItem) {
        mActionItemBar.removeView(actionItem);
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);

        mTabs.setPrivateMode(isPrivate);
        mMenu.setPrivateMode(isPrivate);
        mMenuIcon.setPrivateMode(isPrivate);
        mUrlEditLayout.setPrivateMode(isPrivate);

        if (mBack instanceof BackButton) {
            ((BackButton) mBack).setPrivateMode(isPrivate);
        }

        if (mForward instanceof ForwardButton) {
            ((ForwardButton) mForward).setPrivateMode(isPrivate);
        }
    }

    public void show() {
        setVisibility(View.VISIBLE);
    }

    public void hide() {
        setVisibility(View.GONE);
    }

    public View getDoorHangerAnchor() {
        return mUrlDisplayLayout.getDoorHangerAnchor();
    }

    public void onDestroy() {
        Tabs.unregisterOnTabsChangedListener(this);

        unregisterEventListener("Reader:Click");
        unregisterEventListener("Reader:LongClick");
    }

    public boolean openOptionsMenu() {
        if (!mHasSoftMenuButton)
            return false;

        // Initialize the popup.
        if (mMenuPopup == null) {
            View panel = mActivity.getMenuPanel();
            mMenuPopup = new MenuPopup(mActivity);
            mMenuPopup.setPanelView(panel);

            mMenuPopup.setOnDismissListener(new PopupWindow.OnDismissListener() {
                @Override
                public void onDismiss() {
                    mActivity.onOptionsMenuClosed(null);
                }
            });
        }

        GeckoAppShell.getGeckoInterface().invalidateOptionsMenu();
        if (!mMenuPopup.isShowing())
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

    private void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, this);
    }

    private void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        Log.d(LOGTAG, "handleMessage: " + event);
        if (event.equals("Reader:Click")) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                tab.toggleReaderMode();
            }
        } else if (event.equals("Reader:LongClick")) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                tab.addToReadingList();
            }
        }
    }

    @Override
    public void onLightweightThemeChanged() {
        Drawable drawable = mTheme.getDrawable(this);
        if (drawable == null)
            return;

        StateListDrawable stateList = new StateListDrawable();
        stateList.addState(PRIVATE_STATE_SET, getColorDrawable(R.color.background_private));
        stateList.addState(EMPTY_STATE_SET, drawable);

        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.url_bar_bg);
    }
}
