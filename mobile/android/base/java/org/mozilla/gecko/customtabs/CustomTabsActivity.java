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
import android.provider.Browser;
import android.support.annotation.NonNull;
import android.support.annotation.StyleRes;
import android.support.design.widget.Snackbar;
import android.support.v4.util.SparseArrayCompat;
import android.support.v7.app.ActionBar;
import android.support.v7.view.ActionMode;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.ProgressBar;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SnackbarBuilder;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.gfx.DynamicToolbarAnimator;
import org.mozilla.gecko.gfx.DynamicToolbarAnimator.PinReason;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuInflater;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.IntentUtils;
import org.mozilla.gecko.widget.ActionModePresenter;
import org.mozilla.gecko.widget.GeckoPopupMenu;

import java.util.List;

public class CustomTabsActivity extends GeckoApp implements Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "CustomTabsActivity";
    private static final String SAVED_START_INTENT = "saved_intent_which_started_this_activity";

    private final SparseArrayCompat<PendingIntent> menuItemsIntent = new SparseArrayCompat<>();
    private GeckoPopupMenu popupMenu;
    private View doorhangerOverlay;
    private ActionBarPresenter actionBarPresenter;
    private ProgressBar mProgressView;
    // A state to indicate whether this activity is finishing with customize animation
    private boolean usingCustomAnimation = false;

    // Bug 1351605 - getIntent() not always returns the intent which started this activity.
    // Therefore we make a copy in case of this Activity is re-created.
    private Intent startIntent;

    private MenuItem menuItemControl;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState != null) {
            startIntent = savedInstanceState.getParcelable(SAVED_START_INTENT);
        } else {
            sendTelemetry();
            startIntent = getIntent();
            final String host = getReferrerHost();
            recordCustomTabUsage(host);
        }

        setThemeFromToolbarColor();

        doorhangerOverlay = findViewById(R.id.custom_tabs_doorhanger_overlay);

        mProgressView = (ProgressBar) findViewById(R.id.page_progress);
        final Toolbar toolbar = (Toolbar) findViewById(R.id.actionbar);
        setSupportActionBar(toolbar);
        final ActionBar actionBar = getSupportActionBar();
        bindNavigationCallback(toolbar);

        actionBarPresenter = new ActionBarPresenter(actionBar);
        actionBarPresenter.displayUrlOnly(startIntent.getDataString());
        actionBarPresenter.setBackgroundColor(IntentUtil.getToolbarColor(startIntent), getWindow());
        actionBarPresenter.setTextLongClickListener(new UrlCopyListener());

        Tabs.registerOnTabsChangedListener(this);
    }

    private void sendTelemetry() {
        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "customtab");
        if (IntentUtil.hasToolbarColor(startIntent)) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "customtab-hasToolbarColor");
        }
        if (IntentUtil.hasActionButton(startIntent)) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "customtab-hasActionButton");
        }
        if (IntentUtil.isActionButtonTinted(startIntent)) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "customtab-isActionButtonTinted");
        }
        if (IntentUtil.hasShareItem(startIntent)) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "customtab-hasShareItem");
        }


    }

    private void recordCustomTabUsage(final String host) {
        final GeckoBundle data = new GeckoBundle(1);
        if (host != null) {
            data.putString("client", host);
        } else {
            data.putString("client", "unknown");
        }
        // Pass a message to Gecko to send Telemetry data
        EventDispatcher.getInstance().dispatch("Telemetry:CustomTabsPing", data);
    }

    private void setThemeFromToolbarColor() {
        final int color = ColorUtil.getReadableTextColor(IntentUtil.getToolbarColor(startIntent));
        @StyleRes final int styleRes = (color == Color.BLACK)
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
            return IntentUtil.getAnimationPackageName(startIntent);
        } else {
            return super.getPackageName();
        }
    }

    @Override
    public void finish() {
        super.finish();

        // When 3rd party app launch this Activity, it could also specify custom exit-animation.
        if (IntentUtil.hasExitAnimation(startIntent)) {
            usingCustomAnimation = true;
            overridePendingTransition(IntentUtil.getEnterAnimationRes(startIntent),
                    IntentUtil.getExitAnimationRes(startIntent));
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
    public View getDoorhangerOverlay() {
        return doorhangerOverlay;
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
        outState.putParcelable(SAVED_START_INTENT, startIntent);
    }

    @Override
    public void onResume() {
        if (lastSelectedTabId >= 0) {
            final Tabs tabs = Tabs.getInstance();
            final Tab tab = tabs.getTab(lastSelectedTabId);
            if (tab == null) {
                finish();
            } else {
                // we are restoring
                actionBarPresenter.update(tab);
            }
        }
        super.onResume();

        mLayerView.getDynamicToolbarAnimator().setPinned(true, PinReason.CUSTOM_TAB);
    }

    @Override
    public void onPause() {
        super.onPause();
        mLayerView.getDynamicToolbarAnimator().setPinned(false, PinReason.CUSTOM_TAB);
    }

    // Usually should use onCreateOptionsMenu() to initialize menu items. But GeckoApp overwrite
    // it to support custom menu(Bug 739412). Then the parameter *menu* in this.onCreateOptionsMenu()
    // and this.onPrepareOptionsMenu() are different instances - GeckoApp.onCreatePanelMenu() changed it.
    // CustomTabsActivity only use standard menu in ActionBar, so initialize menu here.
    @Override
    public boolean onCreatePanelMenu(final int id, final Menu menu) {

        // if 3rd-party app asks to add an action button
        if (IntentUtil.hasActionButton(startIntent)) {
            final Bitmap bitmap = IntentUtil.getActionButtonIcon(startIntent);
            final Drawable icon = new BitmapDrawable(getResources(), bitmap);
            final boolean shouldTint = IntentUtil.isActionButtonTinted(startIntent);
            actionBarPresenter.addActionButton(menu, icon, shouldTint)
                    .setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            onActionButtonClicked();
                        }
                    });
        }

        // insert an action button for menu. click it to show popup menu
        popupMenu = createCustomPopupMenu();

        @SuppressWarnings("deprecation")
        Drawable icon = getResources().getDrawable(R.drawable.ab_menu);
        actionBarPresenter.addActionButton(menu, icon, true)
                .setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View anchor) {
                        popupMenu.setAnchor(anchor);
                        popupMenu.show();
                    }
                });

        updateMenuItemForward();
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, "customtab-home");
                finish();
                return true;
            case R.id.share:
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, "customtab-share");
                onShareClicked();
                return true;
            case R.id.custom_tabs_menu_forward:
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, "customtab-forward");
                onForwardClicked();
                return true;
            case R.id.custom_tabs_menu_control:
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, "customtab-control");
                onLoadingControlClicked();
                return true;
            case R.id.custom_tabs_menu_open_in:
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, "customtab-open-in");
                onOpenInClicked();
                return true;
        }

        final PendingIntent intent = menuItemsIntent.get(item.getItemId());
        if (intent != null) {
            onCustomMenuItemClicked(intent);
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    /**
     * Called when the menu that's been clicked is added by the client
     */
    private void onCustomMenuItemClicked(PendingIntent intent) {
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, "customtab-customized-menu");
        performPendingIntent(intent);
    }

    @Override
    protected ActionModePresenter getTextSelectPresenter() {
        return new ActionModePresenter() {
            private ActionMode mMode;

            @Override
            public void startActionMode(ActionMode.Callback callback) {
                mMode = startSupportActionMode(callback);
            }

            @Override
            public void endActionMode() {
                if (mMode != null) {
                    mMode.finish();
                }
            }
        };
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

        // to add custom menu items
        final List<String> titles = IntentUtil.getMenuItemsTitle(startIntent);
        final List<PendingIntent> intents = IntentUtil.getMenuItemsPendingIntent(startIntent);
        menuItemsIntent.clear();
        for (int i = 0; i < titles.size(); i++) {
            final int menuId = Menu.FIRST + i;
            geckoMenu.add(Menu.NONE, menuId, Menu.NONE, titles.get(i));
            menuItemsIntent.put(menuId, intents.get(i));
        }

        // to add share menu item, if necessary
        if (IntentUtil.hasShareItem(startIntent) && !TextUtils.isEmpty(startIntent.getDataString())) {
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

        menuItemControl = geckoMenu.findItem(R.id.custom_tabs_menu_control);

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

        if (menuItemControl != null) {
            Drawable icon = menuItemControl.getIcon();
            icon.setLevel(progress);
        }
    }

    /**
     * Call this method to reload page, or stop page loading if progress not complete yet.
     */
    private void onLoadingControlClicked() {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            if (tab.getLoadProgress() == Tab.LOAD_PROGRESS_STOP) {
                tab.doReload(true);
            } else {
                tab.doStop();
            }
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
            finish();
        }
    }

    private void onActionButtonClicked() {
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, "customtab-action-button");
        PendingIntent pendingIntent = IntentUtil.getActionButtonPendingIntent(startIntent);
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

    /**
     * Listener when user long-click ActionBar to copy URL.
     */
    private class UrlCopyListener implements View.OnLongClickListener {
        @Override
        public boolean onLongClick(View v) {
            final String url = Tabs.getInstance().getSelectedTab().getURL();
            if (!TextUtils.isEmpty(url)) {
                Clipboard.setText(url);
                SnackbarBuilder.builder(CustomTabsActivity.this)
                        .message(R.string.custom_tabs_hint_url_copy)
                        .duration(Snackbar.LENGTH_SHORT)
                        .buildAndShow();
            }
            return true;
        }
    }

    private String getReferrerHost() {
        final Intent intent = this.getIntent();
        String applicationId = IntentUtils.getStringExtraSafe(intent, Browser.EXTRA_APPLICATION_ID);
        if (applicationId != null) {
            return applicationId;
        }
        Uri referrer = intent.getParcelableExtra("android.intent.extra.REFERRER");
        if (referrer != null) {
            return referrer.getHost();
        }
        String referrerName = intent.getStringExtra("android.intent.extra.REFERRER_NAME");
        if (referrerName != null) {
            return Uri.parse(referrerName).getHost();
        }
        return null;
    }
}
