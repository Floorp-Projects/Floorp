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
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.design.widget.Snackbar;
import android.support.v4.util.SparseArrayCompat;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.ProgressBar;

import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.GeckoViewSettings;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SnackbarBuilder;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuInflater;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.IntentUtils;
import org.mozilla.gecko.widget.GeckoPopupMenu;

import java.util.List;

public class CustomTabsActivity extends AppCompatActivity
                                implements GeckoMenu.Callback,
                                           GeckoView.ContentListener,
                                           GeckoView.NavigationListener,
                                           GeckoView.ProgressListener {

    private static final String LOGTAG = "CustomTabsActivity";

    private final SparseArrayCompat<PendingIntent> menuItemsIntent = new SparseArrayCompat<>();
    private GeckoPopupMenu popupMenu;
    private View doorhangerOverlay;
    private ActionBarPresenter actionBarPresenter;
    private ProgressBar mProgressView;
    // A state to indicate whether this activity is finishing with customize animation
    private boolean usingCustomAnimation = false;

    private MenuItem menuItemControl;

    private GeckoView mGeckoView;
    private PromptService mPromptService;

    private boolean mCanGoBack = false;
    private boolean mCanGoForward = false;
    private boolean mCanStop = false;
    private String mCurrentUrl;
    private String mCurrentTitle;
    private int mSecurityStatus = GeckoView.ProgressListener.STATE_IS_INSECURE;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.customtabs_activity);

        final SafeIntent intent = new SafeIntent(getIntent());

        doorhangerOverlay = findViewById(R.id.custom_tabs_doorhanger_overlay);

        mProgressView = (ProgressBar) findViewById(R.id.page_progress);
        updateProgress(10);
        final Toolbar toolbar = (Toolbar) findViewById(R.id.actionbar);
        setSupportActionBar(toolbar);
        final ActionBar actionBar = getSupportActionBar();
        bindNavigationCallback(toolbar);

        actionBarPresenter = new ActionBarPresenter(actionBar, getActionBarTextColor());
        actionBarPresenter.displayUrlOnly(intent.getDataString());
        actionBarPresenter.setBackgroundColor(IntentUtil.getToolbarColor(intent), getWindow());
        actionBarPresenter.setTextLongClickListener(new UrlCopyListener());

        mGeckoView = (GeckoView) findViewById(R.id.gecko_view);

        mGeckoView.setNavigationListener(this);
        mGeckoView.setProgressListener(this);
        mGeckoView.setContentListener(this);

        mPromptService = new PromptService(this, mGeckoView.getEventDispatcher());

        final GeckoViewSettings settings = mGeckoView.getSettings();
        settings.setBoolean(GeckoViewSettings.USE_MULTIPROCESS, false);

        if (intent != null && !TextUtils.isEmpty(intent.getDataString())) {
            mGeckoView.loadUri(intent.getDataString());
        } else {
            Log.w(LOGTAG, "No intend found for custom tab");
            finish();
        }
    }

    @Override
    public void onDestroy() {
        mPromptService.destroy();

        super.onDestroy();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (!ActivityHandlerHelper.handleActivityResult(requestCode, resultCode, data)) {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    private void sendTelemetry() {
        final SafeIntent startIntent = new SafeIntent(getIntent());

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

    @ColorInt
    private int getActionBarTextColor() {
        return ColorUtil.getReadableTextColor(IntentUtil.getToolbarColor(new SafeIntent(getIntent())));
    }

    // Bug 1329145: 3rd party app could specify customized exit-animation to this activity.
    // Activity.overridePendingTransition will invoke getPackageName to retrieve that animation resource.
    // In that case, to return different package name to get customized animation resource.
    @Override
    public String getPackageName() {
        if (usingCustomAnimation) {
            // Use its package name to retrieve animation resource
            return IntentUtil.getAnimationPackageName(new SafeIntent(getIntent()));
        } else {
            return super.getPackageName();
        }
    }

    @Override
    public void finish() {
        super.finish();

        final SafeIntent intent = new SafeIntent(getIntent());
        // When 3rd party app launch this Activity, it could also specify custom exit-animation.
        if (IntentUtil.hasExitAnimation(intent)) {
            usingCustomAnimation = true;
            overridePendingTransition(IntentUtil.getEnterAnimationRes(intent),
                    IntentUtil.getExitAnimationRes(intent));
            usingCustomAnimation = false;
        }
    }

    @Override
    public void onBackPressed() {
        if (mCanGoBack) {
            mGeckoView.goBack();
        } else {
            finish();
        }
    }

    // Usually should use onCreateOptionsMenu() to initialize menu items. But GeckoApp overwrite
    // it to support custom menu(Bug 739412). Then the parameter *menu* in this.onCreateOptionsMenu()
    // and this.onPrepareOptionsMenu() are different instances - GeckoApp.onCreatePanelMenu() changed it.
    // CustomTabsActivity only use standard menu in ActionBar, so initialize menu here.
    @Override
    public boolean onCreateOptionsMenu(final Menu menu) {

        // if 3rd-party app asks to add an action button
        SafeIntent intent = new SafeIntent(getIntent());
        if (IntentUtil.hasActionButton(intent)) {
            final Bitmap bitmap = IntentUtil.getActionButtonIcon(intent);
            final Drawable icon = new BitmapDrawable(getResources(), bitmap);
            final boolean shouldTint = IntentUtil.isActionButtonTinted(intent);
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
    public boolean onMenuItemClick(MenuItem item) {
        return onOptionsItemSelected(item);
    }

    @Override
    public boolean onMenuItemLongClick(MenuItem item) {
        return false;
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

    private void bindNavigationCallback(@NonNull final Toolbar toolbar) {
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                finish();
            }
        });
    }

    private void performPendingIntent(@NonNull PendingIntent pendingIntent) {
        // bug 1337771: If intent-creator haven't set data url, call send() directly won't work.
        final Intent additional = new Intent();
        additional.setData(Uri.parse(mCurrentUrl));
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
        final SafeIntent intent = new SafeIntent(getIntent());

        // pass to to Activity.onMenuItemClick for consistency.
        popupMenu.setOnMenuItemClickListener(new GeckoPopupMenu.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                return CustomTabsActivity.this.onMenuItemClick(item);
            }
        });

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

        menuItemControl = geckoMenu.findItem(R.id.custom_tabs_menu_control);
        // on some configurations(ie. Android 5.1.1 + Nexus 5), no idea why the state not be enabled
        // if the Drawable is a LevelListDrawable, then the icon color is incorrect.
        final Drawable icon = menuItemControl.getIcon();
        if (icon != null && !icon.isStateful()) {
            icon.setState(new int[]{android.R.attr.state_enabled});
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
        forwardMenuItem.setEnabled(mCanGoForward);
    }

    /**
     * Update the state of the progress bar.
     * @param progress The current loading progress; must be between 0 and 100
     */
    private void updateProgress(final int progress) {
        if (mCanStop) {
            mProgressView.setVisibility(View.VISIBLE);
            mProgressView.setProgress(progress);
        } else {
            mProgressView.setVisibility(View.GONE);
        }
    }

    /**
     * Update loading status of current page
     */
    private void updateCanStop() {
        if (menuItemControl != null) {
            Drawable icon = menuItemControl.getIcon();
            if (mCanStop) {
                icon.setLevel(0);
            } else {
                icon.setLevel(100);
            }
        }
    }

    /**
     * Update the state of the action bar
     */
    private void updateActionBar() {
        actionBarPresenter.update(mCurrentTitle, mCurrentUrl, mSecurityStatus);
    }

    /**
     * Call this method to reload page, or stop page loading if progress not complete yet.
     */
    private void onLoadingControlClicked() {
        if (mCanStop) {
            mGeckoView.stop();
        } else {
            mGeckoView.reload();
        }
    }

    private void onForwardClicked() {
        if (mCanGoForward) {
            mGeckoView.goForward();
        }
    }

    /**
     * Callback for Open-in menu item.
     */
    private void onOpenInClicked() {
        final Intent intent = new Intent();
        intent.setData(Uri.parse(mCurrentUrl));
        intent.setAction(Intent.ACTION_VIEW);
        startActivity(intent);
        finish();
    }

    private void onActionButtonClicked() {
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, "customtab-action-button");
        PendingIntent pendingIntent = IntentUtil.getActionButtonPendingIntent(new SafeIntent(getIntent()));
        performPendingIntent(pendingIntent);
    }


    /**
     * Callback for Share menu item.
     */
    private void onShareClicked() {
        if (!TextUtils.isEmpty(mCurrentUrl)) {
            Intent shareIntent = new Intent(Intent.ACTION_SEND);
            shareIntent.setType("text/plain");
            shareIntent.putExtra(Intent.EXTRA_TEXT, mCurrentUrl);

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
            if (!TextUtils.isEmpty(mCurrentUrl)) {
                Clipboard.setText(mCurrentUrl);
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

    /* GeckoView.NavigationListener */
    @Override
    public void onLocationChange(GeckoView view, String url) {
        mCurrentUrl = url;
        updateActionBar();
        updateProgress(60);
    }

    @Override
    public void onCanGoBack(GeckoView view, boolean canGoBack) {
        mCanGoBack = canGoBack;
    }

    @Override
    public void onCanGoForward(GeckoView view, boolean canGoForward) {
        mCanGoForward = canGoForward;
        updateMenuItemForward();
    }

    /* GeckoView.ProgressListener */
    @Override
    public void onPageStart(GeckoView view, String url) {
        mCurrentUrl = url;
        mCanStop = true;
        updateActionBar();
        updateCanStop();
        updateProgress(20);
    }

    @Override
    public void onPageStop(GeckoView view, boolean success) {
        mCanStop = false;
        updateCanStop();
        updateProgress(100);
    }

    @Override
    public void onSecurityChange(GeckoView view, int status, GeckoBundle identity) {
        if ((status & STATE_IS_INSECURE) != 0) {
            mSecurityStatus = STATE_IS_INSECURE;
        } else if ((status & STATE_IS_BROKEN) != 0) {
            mSecurityStatus = STATE_IS_BROKEN;
        } else if ((status & STATE_IS_SECURE) != 0) {
            mSecurityStatus = STATE_IS_SECURE;
        }
        updateActionBar();
    }

    /* GeckoView.ContentListener */
    @Override
    public void onTitleChange(GeckoView view, String title) {
        mCurrentTitle = title;
        updateActionBar();
    }

    @Override
    public void onFullScreen(GeckoView view, boolean fullScreen) {}
}
