/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;

import android.support.annotation.Nullable;
import android.support.v4.content.ContextCompat;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TouchEventInterceptor;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.PropertyAnimator.PropertyAnimationListener;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.lwt.LightweightTheme;
import org.mozilla.gecko.lwt.LightweightThemeDrawable;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.MenuPopup;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.skin.SkinConfig;
import org.mozilla.gecko.tabs.TabHistoryController;
import org.mozilla.gecko.toolbar.ToolbarDisplayLayout.OnStopListener;
import org.mozilla.gecko.toolbar.ToolbarDisplayLayout.OnTitleChangeListener;
import org.mozilla.gecko.toolbar.ToolbarDisplayLayout.UpdateFlags;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.MenuUtils;
import org.mozilla.gecko.widget.themed.ThemedImageButton;
import org.mozilla.gecko.widget.themed.ThemedImageView;
import org.mozilla.gecko.widget.themed.ThemedRelativeLayout;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.support.annotation.NonNull;

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
public abstract class BrowserToolbar extends ThemedRelativeLayout
                                     implements Tabs.OnTabsChangedListener,
                                                GeckoMenu.ActionItemBarPresenter {
    private static final String LOGTAG = "GeckoToolbar";

    private static final int LIGHTWEIGHT_THEME_INVERT_ALPHA = 34; // 255 - alpha = invert_alpha

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

    protected enum UIMode {
        EDIT,
        DISPLAY
    }

    protected final ToolbarDisplayLayout urlDisplayLayout;
    protected final ToolbarEditLayout urlEditLayout;
    protected final View urlBarEntry;
    protected boolean isSwitchingTabs;
    protected final ThemedImageButton tabsButton;

    private ToolbarProgressView progressBar;
    protected final TabCounter tabsCounter;
    protected final View menuButton;
    // bug 1375351: There is no menuIcon in Photon flavor, menuIcon should be removed
    protected final ThemedImageView menuIcon;
    private MenuPopup menuPopup;
    protected final List<View> focusOrder;

    private OnActivateListener activateListener;
    private OnFocusChangeListener focusChangeListener;
    private OnStartEditingListener startEditingListener;
    private OnStopEditingListener stopEditingListener;
    private TouchEventInterceptor mTouchEventInterceptor;

    protected final BrowserApp activity;

    protected UIMode uiMode;
    protected TabHistoryController tabHistoryController;

    private final Paint shadowPaint;
    private final int shadowColor;
    private final int shadowPrivateColor;
    private final int shadowSize;

    private final ToolbarPrefs prefs;

    public abstract boolean isAnimating();

    protected abstract boolean isTabsButtonOffscreen();

    protected abstract void updateNavigationButtons(Tab tab);

    protected abstract void triggerStartEditingTransition(PropertyAnimator animator);
    protected abstract void triggerStopEditingTransition();
    public abstract void triggerTabsPanelTransition(PropertyAnimator animator, boolean areTabsShown);

    /**
     * Returns a Drawable overlaid with the theme's bitmap.
     */
    protected Drawable getLWTDefaultStateSetDrawable() {
        return getTheme().getDrawable(this);
    }

    public static BrowserToolbar create(final Context context, final AttributeSet attrs) {
        final boolean isLargeResource = context.getResources().getBoolean(R.bool.is_large_resource);
        final BrowserToolbar toolbar;
        if (isLargeResource) {
            toolbar = new BrowserToolbarTablet(context, attrs);
        } else {
            toolbar = new BrowserToolbarPhone(context, attrs);
        }
        return toolbar;
    }

    protected BrowserToolbar(final Context context, final AttributeSet attrs) {
        super(context, attrs);
        setWillNotDraw(false);

        // BrowserToolbar is attached to BrowserApp only.
        activity = (BrowserApp) context;

        LayoutInflater.from(context).inflate(R.layout.browser_toolbar, this);

        Tabs.registerOnTabsChangedListener(this);
        isSwitchingTabs = true;

        urlDisplayLayout = (ToolbarDisplayLayout) findViewById(R.id.display_layout);
        urlBarEntry = findViewById(R.id.url_bar_entry);
        urlEditLayout = (ToolbarEditLayout) findViewById(R.id.edit_layout);

        tabsButton = (ThemedImageButton) findViewById(R.id.tabs);
        tabsCounter = (TabCounter) findViewById(R.id.tabs_counter);
        tabsCounter.setLayerType(View.LAYER_TYPE_SOFTWARE, null);

        menuButton = findViewById(R.id.menu);
        menuIcon = (ThemedImageView) findViewById(R.id.menu_icon);

        // The focusOrder List should be filled by sub-classes.
        focusOrder = new ArrayList<View>();

        final Resources res = getResources();
        shadowSize = res.getDimensionPixelSize(R.dimen.browser_toolbar_shadow_size);

        shadowPaint = new Paint();
        shadowColor = ContextCompat.getColor(context, R.color.url_bar_shadow);
        shadowPrivateColor = ContextCompat.getColor(context, R.color.url_bar_shadow_private);
        shadowPaint.setColor(shadowColor);
        shadowPaint.setStrokeWidth(0.0f);

        setUIMode(UIMode.DISPLAY);

        prefs = new ToolbarPrefs();
        urlDisplayLayout.setToolbarPrefs(prefs);
        urlEditLayout.setToolbarPrefs(prefs);

        setOnCreateContextMenuListener(new View.OnCreateContextMenuListener() {
            @Override
            public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
                // Do not show the context menu while editing
                if (isEditing()) {
                    return;
                }

                // NOTE: Use MenuUtils.safeSetVisible because some actions might
                // be on the Page menu
                MenuInflater inflater = activity.getMenuInflater();
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
                        menu.findItem(R.id.set_as_homepage).setVisible(false);
                    }

                    MenuUtils.safeSetVisible(menu, R.id.subscribe, tab.hasFeeds());
                    MenuUtils.safeSetVisible(menu, R.id.add_search_engine, tab.hasOpenSearch());
                    final boolean distSetAsHomepage = GeckoSharedPrefs.forProfile(context).getBoolean(GeckoPreferences.PREFS_SET_AS_HOMEPAGE, false);
                    MenuUtils.safeSetVisible(menu, R.id.set_as_homepage, distSetAsHomepage);
                } else {
                    // if there is no tab, remove anything tab dependent
                    menu.findItem(R.id.copyurl).setVisible(false);
                    menu.findItem(R.id.add_to_launcher).setVisible(false);
                    menu.findItem(R.id.set_as_homepage).setVisible(false);
                    MenuUtils.safeSetVisible(menu, R.id.subscribe, false);
                    MenuUtils.safeSetVisible(menu, R.id.add_search_engine, false);
                }
            }
        });

        setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (activateListener != null) {
                    activateListener.onActivate();
                }
            }
        });
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        prefs.open();

        urlDisplayLayout.setOnStopListener(new OnStopListener() {
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

        urlDisplayLayout.setOnTitleChangeListener(new OnTitleChangeListener() {
            @Override
            public void onTitleChange(CharSequence title) {
                final String contentDescription;
                if (title != null) {
                    contentDescription = title.toString();
                } else {
                    contentDescription = activity.getString(R.string.url_bar_default_text);
                }

                // The title and content description should
                // always be sync.
                setContentDescription(contentDescription);
            }
        });

        urlEditLayout.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                // This will select the url bar when entering editing mode.
                setSelected(hasFocus);
                if (focusChangeListener != null) {
                    focusChangeListener.onFocusChange(v, hasFocus);
                }
            }
        });

        tabsButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Clear focus so a back press with the tabs
                // panel open does not go to the editing field.
                urlEditLayout.clearFocus();

                toggleTabs();
            }
        });
        tabsButton.setImageLevel(0);

        menuButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                // Drop the soft keyboard.
                urlEditLayout.clearFocus();
                activity.openOptionsMenu();
            }
        });
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        prefs.close();
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        final int height = getHeight();
        canvas.drawRect(0, height - shadowSize, getWidth(), height, shadowPaint);
    }

    public void onParentFocus() {
        urlEditLayout.onParentFocus();
    }

    public void setProgressBar(ToolbarProgressView progressBar) {
        this.progressBar = progressBar;
    }

    public void setTabHistoryController(TabHistoryController tabHistoryController) {
        this.tabHistoryController = tabHistoryController;
    }

    public void refresh() {
        progressBar.setImageDrawable(getResources().getDrawable(R.drawable.progress));
        urlDisplayLayout.dismissSiteIdentityPopup();
        urlEditLayout.refresh();
    }

    public boolean onBackPressed() {
        // If we exit editing mode during the animation,
        // we're put into an inconsistent state (bug 1017276).
        if (isEditing() && !isAnimating()) {
            Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL,
                                  TelemetryContract.Method.BACK);
            cancelEdit();
            return true;
        }

        return urlDisplayLayout.dismissSiteIdentityPopup();
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
                    activity.refreshToolbarHeight();
                }
            });
        }
    }

    public void saveTabEditingState(final TabEditingState editingState) {
        urlEditLayout.saveTabEditingState(editingState);
    }

    public void restoreTabEditingState(final TabEditingState editingState) {
        if (!isEditing()) {
            throw new IllegalStateException("Expected to be editing");
        }

        urlEditLayout.restoreTabEditingState(editingState);
    }

    @Override
    public void onTabChanged(@Nullable Tab tab, Tabs.TabEvents msg, String data) {
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
                urlDisplayLayout.dismissSiteIdentityPopup();
                updateTabCount(tabs.getDisplayCount());
                isSwitchingTabs = true;
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
                    if (progressBar.getVisibility() == View.VISIBLE) {
                        progressBar.animateProgress(tab.getLoadProgress());
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
                    updateNavigationButtons(tab);
                    break;

                case SELECTED:
                    flags.add(UpdateFlags.PRIVATE_MODE);
                    setPrivateMode(tab.isPrivate());
                    // Fall through.
                case LOAD_ERROR:
                case LOCATION_CHANGE:
                    // We're displaying the tab URL in place of the title,
                    // so we always need to update our "title" here as well.
                    flags.add(UpdateFlags.TITLE);
                    flags.add(UpdateFlags.FAVICON);
                    flags.add(UpdateFlags.SITE_IDENTITY);

                    updateNavigationButtons(tab);
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

            if (!flags.isEmpty() && tab != null) {
                updateDisplayLayout(tab, flags);
            }
        }

        switch (msg) {
            case SELECTED:
            case LOAD_ERROR:
            case LOCATION_CHANGE:
                isSwitchingTabs = false;
        }
    }

    private void updateProgressVisibility() {
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        // The selected tab may be null if GeckoApp (and thus the
        // selected tab) are not yet initialized (bug 1090287).
        if (selectedTab != null) {
            updateProgressVisibility(selectedTab, selectedTab.getLoadProgress());
        }
    }

    private void updateProgressVisibility(Tab selectedTab, int progress) {
        if (!isEditing() && selectedTab.getState() == Tab.STATE_LOADING) {
            progressBar.setProgress(progress);
            progressBar.setPrivateMode(selectedTab.isPrivate());
            progressBar.setVisibility(View.VISIBLE);
            progressBar.pinDynamicToolbar();
        } else {
            progressBar.setVisibility(View.GONE);
            progressBar.unpinDynamicToolbar();
        }
    }

    protected boolean isVisible() {
        return ViewHelper.getTranslationY(this) == 0;
    }

    @Override
    public void setNextFocusDownId(int nextId) {
        super.setNextFocusDownId(nextId);
        tabsButton.setNextFocusDownId(nextId);
        urlDisplayLayout.setNextFocusDownId(nextId);
        menuButton.setNextFocusDownId(nextId);
    }

    public boolean hideVirtualKeyboard() {
        InputMethodManager imm =
                (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        return imm.hideSoftInputFromWindow(tabsButton.getWindowToken(), 0);
    }

    private void showSelectedTabs() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            if (!tab.isPrivate())
                activity.showNormalTabs();
            else
                activity.showPrivateTabs();
        }
    }

    private void toggleTabs() {
        if (activity.areTabsShown()) {
            return;
        }

        if (hideVirtualKeyboard()) {
            getViewTreeObserver().addOnGlobalLayoutListener(new OnGlobalLayoutListener() {
                @Override
                public void onGlobalLayout() {
                    getViewTreeObserver().removeGlobalOnLayoutListener(this);
                    showSelectedTabs();
                }
            });
        } else {
            showSelectedTabs();
        }
    }

    protected void updateTabCount(final int count) {
        // If toolbar is in edit mode on a phone, this means the entry is expanded
        // and the tabs button is translated offscreen. Don't trigger tabs counter
        // updates until the tabs button is back on screen.
        // See stopEditing()
        if (isTabsButtonOffscreen()) {
            return;
        }

        // Set TabCounter based on visibility
        if (isVisible() && ViewHelper.getAlpha(tabsCounter) != 0 && !isEditing()) {
            tabsCounter.setCountWithAnimation(count);
        } else {
            tabsCounter.setCount(count);
        }

        // Update A11y information
        tabsButton.setContentDescription((count > 1) ?
                                         activity.getString(R.string.num_tabs, count) :
                                         activity.getString(R.string.one_tab));
    }

    private void updateDisplayLayout(@NonNull Tab tab, EnumSet<UpdateFlags> flags) {
        if (isSwitchingTabs) {
            flags.add(UpdateFlags.DISABLE_ANIMATIONS);
        }

        urlDisplayLayout.updateFromTab(tab, flags);

        if (flags.contains(UpdateFlags.TITLE)) {
            if (!isEditing()) {
                urlEditLayout.setText(tab.getURL());
            }
        }

        if (flags.contains(UpdateFlags.PROGRESS)) {
            updateFocusOrder();
        }
    }

    private void updateFocusOrder() {
        if (focusOrder.size() == 0) {
            throw new IllegalStateException("Expected focusOrder to be initialized in subclass");
        }

        View prevView = null;

        // If the element that has focus becomes disabled or invisible, focus
        // is given to the URL bar.
        boolean needsNewFocus = false;

        for (View view : focusOrder) {
            if (view.getVisibility() != View.VISIBLE || !view.isEnabled()) {
                if (view.hasFocus()) {
                    needsNewFocus = true;
                }
                continue;
            }

            if (view.getId() == R.id.menu_items) {
                final LinearLayout actionItemBar = (LinearLayout) view;
                final int childCount = actionItemBar.getChildCount();
                for (int child = 0; child < childCount; child++) {
                    View childView = actionItemBar.getChildAt(child);
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

        urlEditLayout.onEditSuggestion(suggestion);
    }

    public void setTitle(CharSequence title) {
        urlDisplayLayout.setTitle(title);
    }

    public void setOnActivateListener(final OnActivateListener listener) {
        activateListener = listener;
    }

    public void setOnCommitListener(OnCommitListener listener) {
        urlEditLayout.setOnCommitListener(listener);
    }

    public void setOnDismissListener(OnDismissListener listener) {
        urlEditLayout.setOnDismissListener(listener);
    }

    public void setOnFilterListener(OnFilterListener listener) {
        urlEditLayout.setOnFilterListener(listener);
    }

    @Override
    public void setOnFocusChangeListener(OnFocusChangeListener listener) {
        focusChangeListener = listener;
    }

    public void setOnStartEditingListener(OnStartEditingListener listener) {
        startEditingListener = listener;
    }

    public void setOnStopEditingListener(OnStopEditingListener listener) {
        stopEditingListener = listener;
    }

    protected void showUrlEditLayout() {
        setUrlEditLayoutVisibility(true, null);
    }

    protected void showUrlEditLayout(final PropertyAnimator animator) {
        setUrlEditLayoutVisibility(true, animator);
    }

    protected void hideUrlEditLayout() {
        setUrlEditLayoutVisibility(false, null);
    }

    protected void hideUrlEditLayout(final PropertyAnimator animator) {
        setUrlEditLayoutVisibility(false, animator);
    }

    protected void setUrlEditLayoutVisibility(final boolean showEditLayout, PropertyAnimator animator) {
        if (showEditLayout) {
            urlEditLayout.prepareShowAnimation(animator);
        }

        // If this view is GONE, we trigger a measure pass when setting the view to
        // VISIBLE. Since this will occur during the toolbar open animation, it causes jank.
        final int hiddenViewVisibility = View.INVISIBLE;

        if (animator == null) {
            final View viewToShow = (showEditLayout ? urlEditLayout : urlDisplayLayout);
            final View viewToHide = (showEditLayout ? urlDisplayLayout : urlEditLayout);

            viewToHide.setVisibility(hiddenViewVisibility);
            viewToShow.setVisibility(View.VISIBLE);
            return;
        }

        animator.addPropertyAnimationListener(new PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                if (!showEditLayout) {
                    urlEditLayout.setVisibility(hiddenViewVisibility);
                    urlDisplayLayout.setVisibility(View.VISIBLE);
                }
            }

            @Override
            public void onPropertyAnimationEnd() {
                if (showEditLayout) {
                    urlDisplayLayout.setVisibility(hiddenViewVisibility);
                    urlEditLayout.setVisibility(View.VISIBLE);
                }
            }
        });
    }

    private void setUIMode(final UIMode uiMode) {
        this.uiMode = uiMode;
        urlEditLayout.setEnabled(uiMode == UIMode.EDIT);
    }

    /**
     * Returns whether or not the URL bar is in editing mode (url bar is expanded, hiding the new
     * tab button). Note that selection state is independent of editing mode.
     */
    public boolean isEditing() {
        return (uiMode == UIMode.EDIT);
    }

    public void startEditing(String url, PropertyAnimator animator) {
        if (isEditing()) {
            return;
        }

        urlEditLayout.setText(url != null ? url : "");

        setUIMode(UIMode.EDIT);

        updateProgressVisibility();

        if (startEditingListener != null) {
            startEditingListener.onStartEditing();
        }

        triggerStartEditingTransition(animator);
    }

    /**
     * Exits edit mode without updating the toolbar title.
     *
     * @return the url that was entered
     */
    public String cancelEdit() {
        Telemetry.stopUISession(TelemetryContract.Session.AWESOMESCREEN);
        return stopEditing();
    }

    /**
     * Exits edit mode, updating the toolbar title with the url that was just entered.
     *
     * @return the url that was entered
     */
    public String commitEdit() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            tab.resetSiteIdentity();
        }

        final String url = stopEditing();
        if (!TextUtils.isEmpty(url)) {
            setTitle(url);
        }
        return url;
    }

    private String stopEditing() {
        final String url = urlEditLayout.getText();
        if (!isEditing()) {
            return url;
        }
        setUIMode(UIMode.DISPLAY);

        if (stopEditingListener != null) {
            stopEditingListener.onStopEditing();
        }

        updateProgressVisibility();
        triggerStopEditingTransition();

        return url;
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);

        tabsButton.setPrivateMode(isPrivate);
        tabsCounter.setPrivateMode(isPrivate);
        urlEditLayout.setPrivateMode(isPrivate);
        urlDisplayLayout.setPrivateMode(isPrivate);

        // bug 1375351: menuButton is a ThemedImageButton in Photon flavor
        if (SkinConfig.isPhoton()) {
            ((ThemedImageButton)menuButton).setPrivateMode(isPrivate);
        } else {
            ((ShapedButtonFrameLayout)menuButton).setPrivateMode(isPrivate);
        }

        shadowPaint.setColor(isPrivate ? shadowPrivateColor : shadowColor);
    }

    public void show() {
        setVisibility(View.VISIBLE);
    }

    public void hide() {
        setVisibility(View.GONE);
    }

    public View getDoorHangerAnchor() {
        return urlDisplayLayout;
    }

    public void onDestroy() {
        Tabs.unregisterOnTabsChangedListener(this);
        urlDisplayLayout.destroy();
    }

    public boolean openOptionsMenu() {
        // Initialize the popup.
        if (menuPopup == null) {
            View panel = activity.getMenuPanel();
            menuPopup = new MenuPopup(activity);
            menuPopup.setPanelView(panel);

            menuPopup.setOnDismissListener(new PopupWindow.OnDismissListener() {
                @Override
                public void onDismiss() {
                    activity.onOptionsMenuClosed(null);
                }
            });
        }

        activity.invalidateOptionsMenu();
        if (!menuPopup.isShowing()) {
            menuPopup.showAsDropDown(menuButton);
        }

        return true;
    }

    public boolean closeOptionsMenu() {
        if (menuPopup != null && menuPopup.isShowing()) {
            menuPopup.dismiss();
        }

        return true;
    }

    @Override
    public void onLightweightThemeChanged() {
        final Drawable drawable = getLWTDefaultStateSetDrawable();
        if (drawable == null) {
            return;
        }

        final StateListDrawable stateList = new StateListDrawable();
        stateList.addState(PRIVATE_STATE_SET, getColorDrawable(R.color.tabs_tray_grey_pressed));
        stateList.addState(EMPTY_STATE_SET, drawable);

        setBackgroundDrawable(stateList);
    }

    public void setTouchEventInterceptor(TouchEventInterceptor interceptor) {
        mTouchEventInterceptor = interceptor;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (mTouchEventInterceptor != null && mTouchEventInterceptor.onInterceptTouchEvent(this, event)) {
            return true;
        }
        return super.onInterceptTouchEvent(event);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.url_bar_bg);
    }

    public static LightweightThemeDrawable getLightweightThemeDrawable(final View view,
            final LightweightTheme theme, final int colorResID) {
        final int color = ContextCompat.getColor(view.getContext(), colorResID);

        final LightweightThemeDrawable drawable = theme.getColorDrawable(view, color);
        if (drawable != null) {
            drawable.setAlpha(LIGHTWEIGHT_THEME_INVERT_ALPHA, LIGHTWEIGHT_THEME_INVERT_ALPHA);
        }

        return drawable;
    }

    public static class TabEditingState {
        // The edited text from the most recent time this tab was unselected.
        protected String lastEditingText;
        protected int selectionStart;
        protected int selectionEnd;

        public boolean isBrowserSearchShown;

        public void copyFrom(final TabEditingState s2) {
            lastEditingText = s2.lastEditingText;
            selectionStart = s2.selectionStart;
            selectionEnd = s2.selectionEnd;

            isBrowserSearchShown = s2.isBrowserSearchShown;
        }

        public boolean isBrowserSearchShown() {
            return isBrowserSearchShown;
        }

        public void setIsBrowserSearchShown(final boolean isShown) {
            isBrowserSearchShown = isShown;
        }
    }
}
