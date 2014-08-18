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
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.LightweightTheme;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
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
import org.mozilla.gecko.widget.ThemedImageButton;
import org.mozilla.gecko.widget.ThemedImageView;
import org.mozilla.gecko.widget.ThemedRelativeLayout;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.graphics.Paint;
import android.graphics.Path;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.Interpolator;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;

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
public class BrowserToolbar extends ThemedRelativeLayout
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

    private ToolbarDisplayLayout urlDisplayLayout;
    private ToolbarEditLayout urlEditLayout;
    private View urlBarEntry;
    private RelativeLayout.LayoutParams urlBarEntryDefaultLayoutParams;
    private RelativeLayout.LayoutParams urlBarEntryShrunkenLayoutParams;
    private ImageView urlBarTranslatingEdge;
    private boolean isSwitchingTabs;
    private ThemedImageButton tabsButton;
    private ImageButton backButton;
    private ImageButton forwardButton;

    private ToolbarProgressView progressBar;
    private TabCounter tabsCounter;
    private ThemedImageButton menuButton;
    private ThemedImageView menuIcon;
    private LinearLayout actionItemBar;
    private MenuPopup menuPopup;
    private List<View> focusOrder;

    private final ThemedImageView editCancel;

    private List<View> tabletDisplayModeViews;
    private boolean hidForwardButtonOnStartEditing;

    private boolean shouldShrinkURLBar;

    private OnActivateListener activateListener;
    private OnFocusChangeListener focusChangeListener;
    private OnStartEditingListener startEditingListener;
    private OnStopEditingListener stopEditingListener;
    private final PropertyAnimator.PropertyAnimationListener showEditingPhoneAnimationListener;
    private final PropertyAnimator.PropertyAnimationListener stopEditingPhoneAnimationListener;

    private final BrowserApp activity;
    private boolean hasSoftMenuButton;

    private UIMode uiMode;
    private boolean isAnimatingEntry;

    private int urlBarViewOffset;
    private int defaultForwardMargin;

    private Path roundCornerShape;
    private Paint roundCornerPaint;

    private final Paint shadowPaint;
    private final int shadowSize;

    private static final Interpolator buttonsInterpolator = new AccelerateInterpolator();

    private static final int FORWARD_ANIMATION_DURATION = 450;

    private final LightweightTheme theme;
    private final ToolbarPrefs prefs;

    public BrowserToolbar(Context context) {
        this(context, null);
    }

    public BrowserToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWillNotDraw(false);

        theme = ((GeckoApplication) context.getApplicationContext()).getLightweightTheme();

        // BrowserToolbar is attached to BrowserApp only.
        activity = (BrowserApp) context;

        // Inflate the content.
        LayoutInflater.from(context).inflate(R.layout.browser_toolbar, this);

        Tabs.registerOnTabsChangedListener(this);
        isSwitchingTabs = true;

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Reader:Click",
            "Reader:LongClick");

        final Resources res = getResources();
        urlBarViewOffset = res.getDimensionPixelSize(R.dimen.url_bar_offset_left);
        defaultForwardMargin = res.getDimensionPixelSize(R.dimen.forward_default_offset);
        urlDisplayLayout = (ToolbarDisplayLayout) findViewById(R.id.display_layout);
        urlBarEntry = findViewById(R.id.url_bar_entry);
        urlEditLayout = (ToolbarEditLayout) findViewById(R.id.edit_layout);

        urlBarEntryDefaultLayoutParams = (RelativeLayout.LayoutParams) urlBarEntry.getLayoutParams();
        // API level 19 adds a RelativeLayout.LayoutParams copy constructor, so we explicitly cast
        // to ViewGroup.MarginLayoutParams to ensure consistency across platforms.
        urlBarEntryShrunkenLayoutParams =
                new RelativeLayout.LayoutParams((ViewGroup.MarginLayoutParams) urlBarEntryDefaultLayoutParams);
        // Note: a shrunken phone layout is not displayed on any known devices,
        // and thus shrunken layout params for phone are not maintained.
        if (HardwareUtils.isTablet()) {
            urlBarEntryShrunkenLayoutParams.addRule(RelativeLayout.ALIGN_RIGHT, R.id.edit_layout);
            urlBarEntryShrunkenLayoutParams.addRule(RelativeLayout.ALIGN_LEFT, R.id.edit_layout);
            urlBarEntryShrunkenLayoutParams.leftMargin = 0;
        }

        // This will clip the translating edge's image at 60% of its width
        urlBarTranslatingEdge = (ImageView) findViewById(R.id.url_bar_translating_edge);
        if (urlBarTranslatingEdge != null) {
            urlBarTranslatingEdge.getDrawable().setLevel(6000);
        }

        tabsButton = (ThemedImageButton) findViewById(R.id.tabs);
        tabsCounter = (TabCounter) findViewById(R.id.tabs_counter);
        if (Versions.feature11Plus) {
            tabsCounter.setLayerType(View.LAYER_TYPE_SOFTWARE, null);
        }

        backButton = (ImageButton) findViewById(R.id.back);
        setButtonEnabled(backButton, false);
        forwardButton = (ImageButton) findViewById(R.id.forward);
        setButtonEnabled(forwardButton, false);

        menuButton = (ThemedImageButton) findViewById(R.id.menu);
        menuIcon = (ThemedImageView) findViewById(R.id.menu_icon);
        actionItemBar = (LinearLayout) findViewById(R.id.menu_items);
        hasSoftMenuButton = !HardwareUtils.hasMenuButton();

        editCancel = (ThemedImageView) findViewById(R.id.edit_cancel);

        // We use different layouts on phones and tablets, so adjust the focus
        // order appropriately.
        focusOrder = new ArrayList<View>();
        if (HardwareUtils.isTablet()) {
            focusOrder.addAll(Arrays.asList(tabsButton, backButton, forwardButton, this));
            focusOrder.addAll(urlDisplayLayout.getFocusOrder());
            focusOrder.addAll(Arrays.asList(actionItemBar, menuButton));
        } else {
            focusOrder.add(this);
            focusOrder.addAll(urlDisplayLayout.getFocusOrder());
            focusOrder.addAll(Arrays.asList(tabsButton, menuButton));
        }

        if (!HardwareUtils.isTablet()) {
            roundCornerShape = new Path();
            roundCornerShape.moveTo(0, 0);
            roundCornerShape.lineTo(30, 0);
            roundCornerShape.cubicTo(0, 0, 0, 0, 0, 30);
            roundCornerShape.lineTo(0, 0);

            roundCornerPaint = new Paint();
            roundCornerPaint.setAntiAlias(true);
            roundCornerPaint.setColor(res.getColor(R.color.background_tabs));
            roundCornerPaint.setStrokeWidth(0.0f);
        }

        shadowSize = res.getDimensionPixelSize(R.dimen.browser_toolbar_shadow_size);

        shadowPaint = new Paint();
        shadowPaint.setColor(res.getColor(R.color.url_bar_shadow));
        shadowPaint.setStrokeWidth(0.0f);

        setUIMode(UIMode.DISPLAY);

        // Create these listeners here, once, to avoid constructing new listeners
        // each time they are set on an animator (i.e. each time the url bar is clicked).
        showEditingPhoneAnimationListener = new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() { /* Do nothing */ }

            @Override
            public void onPropertyAnimationEnd() {
                isAnimatingEntry = false;
            }
        };

        stopEditingPhoneAnimationListener = new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() { /* Do nothing */ }

            @Override
            public void onPropertyAnimationEnd() {
                urlBarTranslatingEdge.setVisibility(View.INVISIBLE);
                if (shouldShrinkURLBar) {
                    urlBarEntry.setLayoutParams(urlBarEntryDefaultLayoutParams);
                }

                PropertyAnimator buttonsAnimator = new PropertyAnimator(300);
                urlDisplayLayout.prepareStopEditingAnimation(buttonsAnimator);
                buttonsAnimator.start();

                isAnimatingEntry = false;

                // Trigger animation to update the tabs counter once the
                // tabs button is back on screen.
                updateTabCountAndAnimate(Tabs.getInstance().getDisplayCount());
            }
        };

        prefs = new ToolbarPrefs();
        urlDisplayLayout.setToolbarPrefs(prefs);
        urlEditLayout.setToolbarPrefs(prefs);
    }

    public ArrayList<View> populateTabletViews() {
        if (!HardwareUtils.isTablet()) {
            // Avoid the runtime and memory overhead for non-tablet devices.
            return null;
        }

        final View[] allTabletDisplayModeViews = new View[] {
            actionItemBar,
            backButton,
            menuButton,
            menuIcon,
            tabsButton,
            tabsCounter,
        };
        final ArrayList<View> listToPopulate = new ArrayList<View>(allTabletDisplayModeViews.length);

        // Some tablet devices do not display all of the Views but instead rely on visibility
        // to hide them. Find and return the ones that are relevant to our device.
        for (final View v : allTabletDisplayModeViews) {
            // These views should all be initialized and we explicitly do not
            // check for null because we may be hiding bugs.
            if (v.getVisibility() == View.VISIBLE) {
                listToPopulate.add(v);
            }
        };
        return listToPopulate;
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        prefs.open();

        setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (activateListener != null) {
                    activateListener.onActivate();
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
                    }

                    MenuUtils.safeSetVisible(menu, R.id.subscribe, tab.hasFeeds());
                    MenuUtils.safeSetVisible(menu, R.id.add_search_engine, tab.hasOpenSearch());
                } else {
                    // if there is no tab, remove anything tab dependent
                    menu.findItem(R.id.copyurl).setVisible(false);
                    menu.findItem(R.id.add_to_launcher).setVisible(false);
                    MenuUtils.safeSetVisible(menu, R.id.subscribe, false);
                    MenuUtils.safeSetVisible(menu, R.id.add_search_engine, false);
                }
            }
        });

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
                toggleTabs();
            }
        });
        tabsButton.setImageLevel(0);

        backButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                Tabs.getInstance().getSelectedTab().doBack();
            }
        });
        backButton.setOnLongClickListener(new Button.OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                return Tabs.getInstance().getSelectedTab().showBackHistory();
            }
        });

        forwardButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                Tabs.getInstance().getSelectedTab().doForward();
            }
        });
        forwardButton.setOnLongClickListener(new Button.OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                return Tabs.getInstance().getSelectedTab().showForwardHistory();
            }
        });

        editCancel.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                // If we exit editing mode during the animation,
                // we're put into an inconsistent state (bug 1017276).
                if (!isAnimatingEntry) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL,
                                          TelemetryContract.Method.ACTIONBAR,
                                          getResources().getResourceEntryName(editCancel.getId()));
                    cancelEdit();
                }
            }
        });

        if (hasSoftMenuButton) {
            menuButton.setVisibility(View.VISIBLE);
            menuIcon.setVisibility(View.VISIBLE);

            menuButton.setOnClickListener(new Button.OnClickListener() {
                @Override
                public void onClick(View view) {
                    activity.openOptionsMenu();
                }
            });
        }
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        prefs.close();
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        if (!HardwareUtils.isTablet() && uiMode == UIMode.DISPLAY) {
            canvas.drawPath(roundCornerShape, roundCornerPaint);
        }

        final int height = getHeight();
        canvas.drawRect(0, height - shadowSize, getWidth(), height, shadowPaint);
    }

    public void setProgressBar(ToolbarProgressView progressBar) {
        this.progressBar = progressBar;
    }

    public void refresh() {
        urlDisplayLayout.dismissSiteIdentityPopup();
    }

    public boolean onBackPressed() {
        // If we exit editing mode during the animation,
        // we're put into an inconsistent state (bug 1017276).
        if (isEditing() && !isAnimatingEntry) {
            Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL,
                                  TelemetryContract.Method.BACK);
            cancelEdit();
            return true;
        }

        return urlDisplayLayout.dismissSiteIdentityPopup();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // If the motion event has occurred below the toolbar (due to the scroll
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
                    activity.refreshToolbarHeight();
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
                isSwitchingTabs = false;
        }
    }

    private void updateProgressVisibility() {
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        updateProgressVisibility(selectedTab, selectedTab.getLoadProgress());
    }

    private void updateProgressVisibility(Tab selectedTab, int progress) {
        if (!isEditing() && selectedTab.getState() == Tab.STATE_LOADING) {
            progressBar.setProgress(progress);
            progressBar.setVisibility(View.VISIBLE);
        } else {
            progressBar.setVisibility(View.GONE);
        }
    }

    private boolean isVisible() {
        return ViewHelper.getTranslationY(this) == 0;
    }

    @Override
    public void setNextFocusDownId(int nextId) {
        super.setNextFocusDownId(nextId);
        tabsButton.setNextFocusDownId(nextId);
        backButton.setNextFocusDownId(nextId);
        forwardButton.setNextFocusDownId(nextId);
        urlDisplayLayout.setNextFocusDownId(nextId);
        menuButton.setNextFocusDownId(nextId);
    }

    /**
     * Returns the number of pixels the url bar translating edge
     * needs to translate to the right to enter its editing mode state.
     * A negative value means the edge must translate to the left.
     */
    private int getUrlBarEntryTranslation() {
        // Find the distance from the right-edge of the url bar (where we're translating from) to
        // the left-edge of the cancel button (where we're translating to; note that the cancel
        // button must be laid out, i.e. not View.GONE).
        return editCancel.getLeft() - urlBarEntry.getRight();
    }

    private int getUrlBarCurveTranslation() {
        return getWidth() - tabsButton.getLeft();
    }

    private boolean canDoBack(Tab tab) {
        return (tab.canDoBack() && !isEditing());
    }

    private boolean canDoForward(Tab tab) {
        return (tab.canDoForward() && !isEditing());
    }

    private void addTab() {
        activity.addTab();
    }

    private void toggleTabs() {
        if (activity.areTabsShown()) {
            if (activity.hasTabsSideBar())
                activity.hideTabs();
        } else {
            // hide the virtual keyboard
            InputMethodManager imm =
                    (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
            imm.hideSoftInputFromWindow(tabsButton.getWindowToken(), 0);

            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                if (!tab.isPrivate())
                    activity.showNormalTabs();
                else
                    activity.showPrivateTabs();
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
            tabsCounter.setCount(count);

            tabsButton.setContentDescription((count > 1) ?
                                             activity.getString(R.string.num_tabs, count) :
                                             activity.getString(R.string.one_tab));
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

    private void updateDisplayLayout(Tab tab, EnumSet<UpdateFlags> flags) {
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

            if (view == actionItemBar) {
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

    public void prepareTabsAnimation(PropertyAnimator animator, boolean tabsAreShown) {
        if (!tabsAreShown) {
            PropertyAnimator buttonsAnimator =
                    new PropertyAnimator(animator.getDuration(), buttonsInterpolator);

            buttonsAnimator.attach(tabsCounter,
                                   PropertyAnimator.Property.ALPHA,
                                   1.0f);

            if (hasSoftMenuButton && !HardwareUtils.isTablet()) {
                buttonsAnimator.attach(menuIcon,
                                       PropertyAnimator.Property.ALPHA,
                                       1.0f);
            }

            buttonsAnimator.start();

            return;
        }

        ViewHelper.setAlpha(tabsCounter, 0.0f);

        if (hasSoftMenuButton && !HardwareUtils.isTablet()) {
            ViewHelper.setAlpha(menuIcon, 0.0f);
        }
    }

    public void finishTabsAnimation(boolean tabsAreShown) {
        if (tabsAreShown) {
            return;
        }

        PropertyAnimator animator = new PropertyAnimator(150);

        animator.attach(tabsCounter,
                        PropertyAnimator.Property.ALPHA,
                        1.0f);

        if (hasSoftMenuButton && !HardwareUtils.isTablet()) {
            animator.attach(menuIcon,
                            PropertyAnimator.Property.ALPHA,
                            1.0f);
        }

        animator.start();
    }

    public void setOnActivateListener(OnActivateListener listener) {
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

    public void setOnFocusChangeListener(OnFocusChangeListener listener) {
        focusChangeListener = listener;
    }

    public void setOnStartEditingListener(OnStartEditingListener listener) {
        startEditingListener = listener;
    }

    public void setOnStopEditingListener(OnStopEditingListener listener) {
        stopEditingListener = listener;
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
        if (showEditLayout) {
            urlEditLayout.prepareShowAnimation(animator);
        }

        if (animator == null) {
            final View viewToShow = (showEditLayout ? urlEditLayout : urlDisplayLayout);
            final View viewToHide = (showEditLayout ? urlDisplayLayout : urlEditLayout);

            viewToHide.setVisibility(View.GONE);
            viewToShow.setVisibility(View.VISIBLE);

            final int cancelVisibility = (showEditLayout ? View.VISIBLE : View.INVISIBLE);
            setCancelVisibility(cancelVisibility);
            return;
        }

        animator.addPropertyAnimationListener(new PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                if (!showEditLayout) {
                    urlEditLayout.setVisibility(View.GONE);
                    urlDisplayLayout.setVisibility(View.VISIBLE);

                    setCancelVisibility(View.INVISIBLE);
                }
            }

            @Override
            public void onPropertyAnimationEnd() {
                if (showEditLayout) {
                    urlDisplayLayout.setVisibility(View.GONE);
                    urlEditLayout.setVisibility(View.VISIBLE);

                    setCancelVisibility(View.VISIBLE);
                }
            }
        });
    }

    private void setCancelVisibility(final int visibility) {
        editCancel.setVisibility(visibility);
    }

    /**
     * Disables all toolbar elements which are not
     * related to editing mode.
     */
    private void updateChildrenEnabledStateForEditing() {
        // Disable toolbar elements while in editing mode
        final boolean enabled = !isEditing();

        if (!enabled) {
            tabsCounter.onEnterEditingMode();
        }

        tabsButton.setEnabled(enabled);
        menuButton.setEnabled(enabled);

        final int actionItemsCount = actionItemBar.getChildCount();
        for (int i = 0; i < actionItemsCount; i++) {
            actionItemBar.getChildAt(i).setEnabled(enabled);
        }

        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            setButtonEnabled(backButton, canDoBack(tab));
            setButtonEnabled(forwardButton, canDoForward(tab));

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

    public boolean isAnimating() {
        return isAnimatingEntry;
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

        if (HardwareUtils.isTablet()) {
            showEditingOnTablet();
        } else {
            urlBarTranslatingEdge.setVisibility(View.VISIBLE);

            final int curveTranslation = getUrlBarCurveTranslation();
            final int entryTranslation = getUrlBarEntryTranslation();
            shouldShrinkURLBar = (entryTranslation < 0);
            if (shouldShrinkURLBar) {
                urlBarEntry.setLayoutParams(urlBarEntryShrunkenLayoutParams);
            }

            if (Versions.preHC) {
                showEditingOnPreHoneycomb(entryTranslation, curveTranslation);
            } else {
                showEditingWithPhoneAnimation(animator, entryTranslation, curveTranslation);
            }
        }
    }

    private void showEditingOnPreHoneycomb(final int entryTranslation,
            final int curveTranslation) {
        showUrlEditLayout();

        // Prevent taps through the editing mode cancel button (bug 1001243).
        tabsButton.setEnabled(false);

        ViewHelper.setTranslationX(urlBarTranslatingEdge, entryTranslation);
        ViewHelper.setTranslationX(tabsButton, curveTranslation);
        ViewHelper.setTranslationX(tabsCounter, curveTranslation);
        ViewHelper.setTranslationX(actionItemBar, curveTranslation);

        if (hasSoftMenuButton) {
            // Prevent tabs through the editing mode cancel button (bug 1001243).
            menuButton.setEnabled(false);

            ViewHelper.setTranslationX(menuButton, curveTranslation);
            ViewHelper.setTranslationX(menuIcon, curveTranslation);
        }
    }

    private void showEditingOnTablet() {
        if (tabletDisplayModeViews == null) {
            tabletDisplayModeViews = populateTabletViews();
        }

        urlBarEntry.setLayoutParams(urlBarEntryShrunkenLayoutParams);

        // Hide display elements.
        updateChildrenEnabledStateForEditing();
        for (final View v : tabletDisplayModeViews) {
            v.setVisibility(View.INVISIBLE);
        }

        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab != null && selectedTab.canDoForward()) {
            hidForwardButtonOnStartEditing = true;
            forwardButton.setVisibility(View.INVISIBLE);
        } else {
            hidForwardButtonOnStartEditing = false;
        }

        // Show editing elements.
        showUrlEditLayout();
    }

    private void showEditingWithPhoneAnimation(final PropertyAnimator animator,
            final int entryTranslation, final int curveTranslation) {
        if (isAnimatingEntry)
            return;

        urlDisplayLayout.prepareStartEditingAnimation();

        // Slide toolbar elements.
        animator.attach(urlBarTranslatingEdge,
                        PropertyAnimator.Property.TRANSLATION_X,
                        entryTranslation);
        animator.attach(tabsButton,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);
        animator.attach(tabsCounter,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);
        animator.attach(actionItemBar,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);

        if (hasSoftMenuButton) {
            animator.attach(menuButton,
                            PropertyAnimator.Property.TRANSLATION_X,
                            curveTranslation);

            animator.attach(menuIcon,
                            PropertyAnimator.Property.TRANSLATION_X,
                            curveTranslation);
        }

        showUrlEditLayout(animator);

        animator.addPropertyAnimationListener(showEditingPhoneAnimationListener);
        isAnimatingEntry = true; // To be correct, this should be called last.
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

        updateChildrenEnabledStateForEditing();

        if (stopEditingListener != null) {
            stopEditingListener.onStopEditing();
        }

        updateProgressVisibility();

        // The animation looks cleaner if the text in the URL bar is
        // not selected so clear the selection by clearing focus.
        urlEditLayout.clearFocus();

        if (Versions.preHC) {
            stopEditingOnPreHoneycomb();
        } else if (HardwareUtils.isTablet()) {
            stopEditingOnTablet();
        } else {
            stopEditingWithPhoneAnimation();
        }

        return url;
    }

    private void stopEditingOnPreHoneycomb() {
        hideUrlEditLayout();

        updateTabCountAndAnimate(Tabs.getInstance().getDisplayCount());

        if (shouldShrinkURLBar) {
            urlBarEntry.setLayoutParams(urlBarEntryDefaultLayoutParams);
        }

        tabsButton.setEnabled(true);

        urlBarTranslatingEdge.setVisibility(View.INVISIBLE);
        ViewHelper.setTranslationX(urlBarTranslatingEdge, 0);
        ViewHelper.setTranslationX(tabsButton, 0);
        ViewHelper.setTranslationX(tabsCounter, 0);
        ViewHelper.setTranslationX(actionItemBar, 0);

        if (hasSoftMenuButton) {
            menuButton.setEnabled(true);

            ViewHelper.setTranslationX(menuButton, 0);
            ViewHelper.setTranslationX(menuIcon, 0);
        }
    }

    private void stopEditingOnTablet() {
        if (tabletDisplayModeViews == null) {
            throw new IllegalStateException("We initialize tabletDisplayModeViews in the " +
                    "transition to show editing mode and don't expect stop editing to be called " +
                    "first.");
        }

        urlBarEntry.setLayoutParams(urlBarEntryDefaultLayoutParams);

        // Show display elements.
        updateChildrenEnabledStateForEditing();
        for (final View v : tabletDisplayModeViews) {
            v.setVisibility(View.VISIBLE);
        }

        if (hidForwardButtonOnStartEditing) {
            forwardButton.setVisibility(View.VISIBLE);
        }

        // Hide editing elements.
        hideUrlEditLayout();
    }

    private void stopEditingWithPhoneAnimation() {
        final PropertyAnimator contentAnimator = new PropertyAnimator(250);
        contentAnimator.setUseHardwareLayer(false);

        // Slide the toolbar back to its original size.
        contentAnimator.attach(urlBarTranslatingEdge,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(tabsButton,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(tabsCounter,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);
        contentAnimator.attach(actionItemBar,
                               PropertyAnimator.Property.TRANSLATION_X,
                               0);

        if (hasSoftMenuButton) {
            contentAnimator.attach(menuButton,
                                   PropertyAnimator.Property.TRANSLATION_X,
                                   0);

            contentAnimator.attach(menuIcon,
                                   PropertyAnimator.Property.TRANSLATION_X,
                                   0);
        }

        hideUrlEditLayout(contentAnimator);

        contentAnimator.addPropertyAnimationListener(stopEditingPhoneAnimationListener);

        isAnimatingEntry = true;
        contentAnimator.start();
    }

    private void setButtonEnabled(ImageButton button, boolean enabled) {
        final Drawable drawable = button.getDrawable();
        if (drawable != null) {
            drawable.setAlpha(enabled ? 255 : 61);
        }

        button.setEnabled(enabled);
    }

    public void updateBackButton(Tab tab) {
        setButtonEnabled(backButton, canDoBack(tab));
    }

    private void animateForwardButton(final ForwardButtonAnimation animation) {
        // If the forward button is not visible, we must be
        // in the phone UI.
        if (forwardButton.getVisibility() != View.VISIBLE) {
            return;
        }

        final boolean showing = (animation == ForwardButtonAnimation.SHOW);

        // if the forward button's margin is non-zero, this means it has already
        // been animated to be visible¸ and vice-versa.
        MarginLayoutParams fwdParams = (MarginLayoutParams) forwardButton.getLayoutParams();
        if ((fwdParams.leftMargin > defaultForwardMargin && showing) ||
            (fwdParams.leftMargin == defaultForwardMargin && !showing)) {
            return;
        }

        // We want the forward button to show immediately when switching tabs
        final PropertyAnimator forwardAnim =
                new PropertyAnimator(isSwitchingTabs ? 10 : FORWARD_ANIMATION_DURATION);
        final int width = forwardButton.getWidth() / 2;

        forwardAnim.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                if (!showing) {
                    // Set the margin before the transition when hiding the forward button. We
                    // have to do this so that the favicon isn't clipped during the transition
                    MarginLayoutParams layoutParams =
                        (MarginLayoutParams) urlDisplayLayout.getLayoutParams();
                    layoutParams.leftMargin = 0;

                    // Do the same on the URL edit container
                    layoutParams = (MarginLayoutParams) urlEditLayout.getLayoutParams();
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
                        (MarginLayoutParams) urlDisplayLayout.getLayoutParams();
                    layoutParams.leftMargin = urlBarViewOffset;

                    layoutParams = (MarginLayoutParams) urlEditLayout.getLayoutParams();
                    layoutParams.leftMargin = urlBarViewOffset;
                }

                urlDisplayLayout.finishForwardAnimation();

                MarginLayoutParams layoutParams = (MarginLayoutParams) forwardButton.getLayoutParams();
                layoutParams.leftMargin = defaultForwardMargin + (showing ? width : 0);
                ViewHelper.setTranslationX(forwardButton, 0);

                requestLayout();
            }
        });

        prepareForwardAnimation(forwardAnim, animation, width);
        forwardAnim.start();
    }

    public void updateForwardButton(Tab tab) {
        final boolean enabled = canDoForward(tab);
        if (forwardButton.isEnabled() == enabled)
            return;

        // Save the state on the forward button so that we can skip animations
        // when there's nothing to change
        setButtonEnabled(forwardButton, enabled);
        animateForwardButton(enabled ? ForwardButtonAnimation.SHOW : ForwardButtonAnimation.HIDE);
    }

    private void prepareForwardAnimation(PropertyAnimator anim, ForwardButtonAnimation animation, int width) {
        if (animation == ForwardButtonAnimation.HIDE) {
            anim.attach(forwardButton,
                      PropertyAnimator.Property.TRANSLATION_X,
                      -width);
            anim.attach(forwardButton,
                      PropertyAnimator.Property.ALPHA,
                      0);

        } else {
            anim.attach(forwardButton,
                      PropertyAnimator.Property.TRANSLATION_X,
                      width);
            anim.attach(forwardButton,
                      PropertyAnimator.Property.ALPHA,
                      1);
        }

        urlDisplayLayout.prepareForwardAnimation(anim, animation, width);
    }

    @Override
    public boolean addActionItem(View actionItem) {
        actionItemBar.addView(actionItem);
        return true;
    }

    @Override
    public void removeActionItem(View actionItem) {
        actionItemBar.removeView(actionItem);
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);

        tabsButton.setPrivateMode(isPrivate);
        menuButton.setPrivateMode(isPrivate);
        menuIcon.setPrivateMode(isPrivate);
        editCancel.setPrivateMode(isPrivate);
        urlEditLayout.setPrivateMode(isPrivate);

        if (backButton instanceof BackButton) {
            ((BackButton) backButton).setPrivateMode(isPrivate);
        }

        if (forwardButton instanceof ForwardButton) {
            ((ForwardButton) forwardButton).setPrivateMode(isPrivate);
        }
    }

    public void show() {
        setVisibility(View.VISIBLE);
    }

    public void hide() {
        setVisibility(View.GONE);
    }

    public View getDoorHangerAnchor() {
        return urlDisplayLayout.getDoorHangerAnchor();
    }

    public void onDestroy() {
        Tabs.unregisterOnTabsChangedListener(this);

        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "Reader:Click",
            "Reader:LongClick");
    }

    public boolean openOptionsMenu() {
        if (!hasSoftMenuButton) {
            return false;
        }

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

        GeckoAppShell.getGeckoInterface().invalidateOptionsMenu();
        if (!menuPopup.isShowing()) {
            menuPopup.showAsDropDown(menuButton);
        }

        return true;
    }

    public boolean closeOptionsMenu() {
        if (!hasSoftMenuButton) {
            return false;
        }

        if (menuPopup != null && menuPopup.isShowing()) {
            menuPopup.dismiss();
        }

        return true;
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
        Drawable drawable = theme.getDrawable(this);
        if (drawable == null)
            return;

        StateListDrawable stateList = new StateListDrawable();
        stateList.addState(PRIVATE_STATE_SET, getColorDrawable(R.color.background_private));
        stateList.addState(EMPTY_STATE_SET, drawable);

        setBackgroundDrawable(stateList);

        editCancel.onLightweightThemeChanged();
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.url_bar_bg);
        editCancel.onLightweightThemeReset();
    }
}
