/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.app.PendingIntent;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.StyleRes;
import android.support.annotation.VisibleForTesting;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.util.SparseArrayCompat;
import android.support.v4.view.MenuItemCompat;
import android.support.v7.app.ActionBar;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageButton;
import android.widget.ProgressBar;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuInflater;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.widget.GeckoPopupMenu;

import java.util.List;

import static android.support.customtabs.CustomTabsIntent.EXTRA_TOOLBAR_COLOR;

public class CustomTabsActivity extends GeckoApp implements Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "CustomTabsActivity";
    private static final String SAVED_TOOLBAR_COLOR = "SavedToolbarColor";

    @ColorInt
    private static final int DEFAULT_ACTION_BAR_COLOR = 0xFF363b40; // default color to match design

    private final SparseArrayCompat<PendingIntent> menuItemsIntent = new SparseArrayCompat<>();
    private GeckoPopupMenu popupMenu;
    private ActionBarPresenter actionBarPresenter;
    private ProgressBar mProgressView;
    // A state to indicate whether this activity is finishing with customize animation
    private boolean usingCustomAnimation = false;

    @ColorInt
    private int toolbarColor = DEFAULT_ACTION_BAR_COLOR;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState != null) {
            toolbarColor = savedInstanceState.getInt(SAVED_TOOLBAR_COLOR, DEFAULT_ACTION_BAR_COLOR);
        } else {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "customtab");
            toolbarColor = getIntent().getIntExtra(EXTRA_TOOLBAR_COLOR, DEFAULT_ACTION_BAR_COLOR);
        }

        // Translucent color does not make sense for toolbar color. Ensure it is 0xFF.
        toolbarColor = 0xFF000000 | toolbarColor;

        setThemeFromToolbarColor();

        mProgressView = (ProgressBar) findViewById(R.id.page_progress);
        final Toolbar toolbar = (Toolbar) findViewById(R.id.actionbar);
        setSupportActionBar(toolbar);
        final ActionBar actionBar = getSupportActionBar();
        bindNavigationCallback(toolbar);

        actionBarPresenter = new ActionBarPresenter(actionBar);
        actionBarPresenter.displayUrlOnly(getIntent().getDataString());
        actionBarPresenter.setBackgroundColor(toolbarColor, getWindow());
        actionBar.setDisplayHomeAsUpEnabled(true);

        Tabs.registerOnTabsChangedListener(this);
    }

    private void setThemeFromToolbarColor() {
        @StyleRes
        int styleRes = (ColorUtil.getReadableTextColor(toolbarColor) == Color.BLACK)
                ? R.style.GeckoCustomTabs_Light
                : R.style.GeckoCustomTabs;

        setTheme(styleRes);
    }

    // Bug 1329145: 3rd party app could specify customized exit-animation to this activity.
    // Activity.overridePendingTransition will invoke getPackageName to retrieve that animation resource.
    // In that case, to return different package name to get customized animation resource.
    @Override
    public String getPackageName() {
        if (usingCustomAnimation) {
            // Use its package name to retrieve animation resource
            return IntentUtil.getAnimationPackageName(getIntent());
        } else {
            return super.getPackageName();
        }
    }

    @Override
    public void finish() {
        super.finish();

        // When 3rd party app launch this Activity, it could also specify custom exit-animation.
        if (IntentUtil.hasExitAnimation(getIntent())) {
            usingCustomAnimation = true;
            overridePendingTransition(IntentUtil.getEnterAnimationRes(getIntent()),
                    IntentUtil.getExitAnimationRes(getIntent()));
            usingCustomAnimation = false;
        }
    }

    @Override
    protected int getNewTabFlags() {
        return Tabs.LOADURL_CUSTOMTAB | super.getNewTabFlags();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Tabs.unregisterOnTabsChangedListener(this);
    }

    @Override
    public int getLayout() {
        return R.layout.customtabs_activity;
    }

    @Override
    protected void onDone() {
        finish();
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        if (!Tabs.getInstance().isSelectedTab(tab)) {
            return;
        }

        if (msg == Tabs.TabEvents.START
                || msg == Tabs.TabEvents.STOP
                || msg == Tabs.TabEvents.ADDED
                || msg == Tabs.TabEvents.LOAD_ERROR
                || msg == Tabs.TabEvents.LOADED
                || msg == Tabs.TabEvents.LOCATION_CHANGE) {

            updateProgress((tab.getState() == Tab.STATE_LOADING),
                    tab.getLoadProgress());
        }

        if (msg == Tabs.TabEvents.LOCATION_CHANGE
                || msg == Tabs.TabEvents.SECURITY_CHANGE
                || msg == Tabs.TabEvents.TITLE) {
            actionBarPresenter.update(tab);
        }

        updateMenuItemForward();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putInt(SAVED_TOOLBAR_COLOR, toolbarColor);
    }

    @Override
    public void onResume() {
        if (lastSelectedTabId >= 0) {
            final Tabs tabs = Tabs.getInstance();
            final Tab tab = tabs.getTab(lastSelectedTabId);
            if (tab == null) {
                finish();
            }
        }
        super.onResume();
    }

    // Usually should use onCreateOptionsMenu() to initialize menu items. But GeckoApp overwrite
    // it to support custom menu(Bug 739412). Then the parameter *menu* in this.onCreateOptionsMenu()
    // and this.onPrepareOptionsMenu() are different instances - GeckoApp.onCreatePanelMenu() changed it.
    // CustomTabsActivity only use standard menu in ActionBar, so initialize menu here.
    @Override
    public boolean onCreatePanelMenu(final int id, final Menu menu) {
        insertActionButton(menu, getIntent(), actionBarPresenter.getTextPrimaryColor());

        popupMenu = createCustomPopupMenu();

        // Create a ImageButton manually, and use it as an anchor for PopupMenu.
        final ImageButton btn = new ImageButton(getContext(),
                null, 0, R.style.Widget_MenuButtonCustomTabs);
        btn.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View anchor) {
                popupMenu.setAnchor(anchor);
                popupMenu.show();
            }
        });

        // Insert the anchor-button to Menu
        final MenuItem item = menu.add(Menu.NONE, R.id.menu, Menu.NONE, "Menu Button");
        item.setActionView(btn);
        MenuItemCompat.setShowAsAction(item, MenuItemCompat.SHOW_AS_ACTION_ALWAYS);

        updateMenuItemForward();
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
                return true;
            case R.id.action_button:
                onActionButtonClicked();
                return true;
            case R.id.share:
                onShareClicked();
                return true;
            case R.id.custom_tabs_menu_forward:
                onForwardClicked();
                return true;
            case R.id.custom_tabs_menu_reload:
                onReloadClicked();
                return true;
            case R.id.custom_tabs_menu_open_in:
                onOpenInClicked();
                return true;
        }

        final PendingIntent intent = menuItemsIntent.get(item.getItemId());
        if (intent != null) {
            performPendingIntent(intent);
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    /**
     * To insert a MenuItem (as an ActionButton) into Menu.
     *
     * @param menu   The options menu in which to place items.
     * @param intent which to launch this activity
     * @param tintColor color to tint action-button
     * @return the MenuItem which be created and inserted into menu. Otherwise, null.
     */
    @VisibleForTesting
    MenuItem insertActionButton(final Menu menu,
                                final Intent intent,
                                @ColorInt final int tintColor) {
        if (!IntentUtil.hasActionButton(intent)) {
            return null;
        }

        MenuItem item = menu.add(Menu.NONE,
                R.id.action_button,
                Menu.NONE,
                IntentUtil.getActionButtonDescription(intent));
        Bitmap bitmap = IntentUtil.getActionButtonIcon(intent);
        final Drawable icon = new BitmapDrawable(getResources(), bitmap);
        if (IntentUtil.isActionButtonTinted(intent)) {
            DrawableCompat.setTint(icon, tintColor);
        }
        item.setIcon(icon);
        MenuItemCompat.setShowAsAction(item, MenuItemCompat.SHOW_AS_ACTION_ALWAYS);

        return item;
    }

    private void bindNavigationCallback(@NonNull final Toolbar toolbar) {
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final Tabs tabs = Tabs.getInstance();
                final Tab tab = tabs.getSelectedTab();
                tabs.closeTab(tab);
                finish();
            }
        });
    }

    private void performPendingIntent(@NonNull PendingIntent pendingIntent) {
        // bug 1337771: If intent-creator haven't set data url, call send() directly won't work.
        final Intent additional = new Intent();
        final Tab tab = Tabs.getInstance().getSelectedTab();
        additional.setData(Uri.parse(tab.getURL()));
        try {
            pendingIntent.send(this, 0, additional);
        } catch (PendingIntent.CanceledException e) {
            Log.w(LOGTAG, "Performing a canceled pending intent", e);
        }
    }

    /**
     * To generate a popup menu which looks like an ordinary option menu, but have extra elements
     * such as footer.
     *
     * @return a GeckoPopupMenu which can be placed on any view.
     */
    private GeckoPopupMenu createCustomPopupMenu() {
        final GeckoPopupMenu popupMenu = new GeckoPopupMenu(this);
        final GeckoMenu geckoMenu = popupMenu.getMenu();

        // pass to to Activity.onMenuItemClick for consistency.
        popupMenu.setOnMenuItemClickListener(new GeckoPopupMenu.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                return CustomTabsActivity.this.onMenuItemClick(item);
            }
        });

        // to add Fennec default menu
        final Intent intent = getIntent();

        // to add custom menu items
        final List<String> titles = IntentUtil.getMenuItemsTitle(intent);
        final List<PendingIntent> intents = IntentUtil.getMenuItemsPendingIntent(intent);
        menuItemsIntent.clear();
        for (int i = 0; i < titles.size(); i++) {
            final int menuId = Menu.FIRST + i;
            geckoMenu.add(Menu.NONE, menuId, Menu.NONE, titles.get(i));
            menuItemsIntent.put(menuId, intents.get(i));
        }

        // to add share menu item, if necessary
        if (IntentUtil.hasShareItem(intent) && !TextUtils.isEmpty(intent.getDataString())) {
            geckoMenu.add(Menu.NONE, R.id.share, Menu.NONE, getString(R.string.share));
        }

        final MenuInflater inflater = new GeckoMenuInflater(this);
        inflater.inflate(R.menu.customtabs_menu, geckoMenu);

        // insert default browser name to title of menu-item-Open-In
        final MenuItem openItem = geckoMenu.findItem(R.id.custom_tabs_menu_open_in);
        if (openItem != null) {
            final Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://"));
            final ResolveInfo info = getPackageManager()
                    .resolveActivity(browserIntent, PackageManager.MATCH_DEFAULT_ONLY);
            final String name = info.loadLabel(getPackageManager()).toString();
            openItem.setTitle(getString(R.string.custom_tabs_menu_item_open_in, name));
        }

        geckoMenu.addFooterView(
                getLayoutInflater().inflate(R.layout.customtabs_options_menu_footer, geckoMenu, false),
                null,
                false);

        return popupMenu;
    }

    /**
     * Update state of Forward button in Popup Menu. It is clickable only if current tab can do forward.
     */
    private void updateMenuItemForward() {
        if ((popupMenu == null)
                || (popupMenu.getMenu() == null)
                || (popupMenu.getMenu().findItem(R.id.custom_tabs_menu_forward) == null)) {
            return;
        }

        final MenuItem forwardMenuItem = popupMenu.getMenu().findItem(R.id.custom_tabs_menu_forward);
        final Tab tab = Tabs.getInstance().getSelectedTab();
        final boolean enabled = (tab != null && tab.canDoForward());
        forwardMenuItem.setEnabled(enabled);
    }

    /**
     * Update loading progress of current page
     *
     * @param isLoading to indicate whether ProgressBar should be visible or not
     * @param progress  value of loading progress in percent, should be 0 - 100.
     */
    private void updateProgress(final boolean isLoading, final int progress) {
        if (isLoading) {
            mProgressView.setVisibility(View.VISIBLE);
            mProgressView.setProgress(progress);
        } else {
            mProgressView.setVisibility(View.GONE);
        }
    }

    private void onReloadClicked() {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            tab.doReload(true);
        }
    }

    private void onForwardClicked() {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        if ((tab != null) && tab.canDoForward()) {
            tab.doForward();
        }
    }

    /**
     * Callback for Open-in menu item.
     */
    private void onOpenInClicked() {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            // To launch default browser with url of current tab.
            final Intent intent = new Intent();
            intent.setData(Uri.parse(tab.getURL()));
            intent.setAction(Intent.ACTION_VIEW);
            startActivity(intent);
        }
    }

    private void onActionButtonClicked() {
        PendingIntent pendingIntent = IntentUtil.getActionButtonPendingIntent(getIntent());
        performPendingIntent(pendingIntent);
    }

    /**
     * Callback for Share menu item.
     */
    private void onShareClicked() {
        final String url = Tabs.getInstance().getSelectedTab().getURL();

        if (!TextUtils.isEmpty(url)) {
            Intent shareIntent = new Intent(Intent.ACTION_SEND);
            shareIntent.setType("text/plain");
            shareIntent.putExtra(Intent.EXTRA_TEXT, url);

            Intent chooserIntent = Intent.createChooser(shareIntent, getString(R.string.share_title));
            chooserIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(chooserIntent);
        }
    }
}
