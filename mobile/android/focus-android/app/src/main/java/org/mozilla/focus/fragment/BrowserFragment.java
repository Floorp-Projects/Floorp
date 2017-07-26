/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.Manifest;
import android.app.Activity;
import android.app.DownloadManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.design.widget.AppBarLayout;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.content.ContextCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.CookieManager;
import android.webkit.URLUtil;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InfoActivity;
import org.mozilla.focus.activity.InstallFirefoxActivity;
import org.mozilla.focus.broadcastreceiver.DownloadBroadcastReceiver;
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity;
import org.mozilla.focus.menu.BrowserMenu;
import org.mozilla.focus.menu.WebContextMenu;
import org.mozilla.focus.notification.BrowsingNotificationService;
import org.mozilla.focus.open.OpenWithFragment;
import org.mozilla.focus.shortcut.HomeScreen;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.Browsers;
import org.mozilla.focus.utils.ColorUtils;
import org.mozilla.focus.utils.DrawableUtils;
import org.mozilla.focus.utils.IntentUtils;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.utils.ViewUtils;
import org.mozilla.focus.web.BrowsingSession;
import org.mozilla.focus.web.CustomTabConfig;
import org.mozilla.focus.web.Download;
import org.mozilla.focus.web.IWebView;
import org.mozilla.focus.widget.AnimatedProgressBar;

import java.lang.ref.WeakReference;

/**
 * Fragment for displaying the browser UI.
 */
public class BrowserFragment extends WebFragment implements View.OnClickListener, DownloadDialogFragment.DownloadDialogListener {
    public static final String FRAGMENT_TAG = "browser";

    private static int REQUEST_CODE_STORAGE_PERMISSION = 101;
    private static final int ANIMATION_DURATION = 300;
    private static final String ARGUMENT_URL = "url";
    private static final String RESTORE_KEY_DOWNLOAD = "download";

    public static BrowserFragment create(String url) {
        Bundle arguments = new Bundle();
        arguments.putString(ARGUMENT_URL, url);

        BrowserFragment fragment = new BrowserFragment();
        fragment.setArguments(arguments);

        return fragment;
    }

    private Download pendingDownload;
    private View backgroundView;
    private TransitionDrawable backgroundTransition;
    private TextView urlView;
    private AnimatedProgressBar progressView;
    private FrameLayout blockView;
    private ImageView lockView;
    private ImageButton menuView;
    private WeakReference<BrowserMenu> menuWeakReference = new WeakReference<>(null);

    /**
     * Container for custom video views shown in fullscreen mode.
     */
    private ViewGroup videoContainer;

    /**
     * Container containing the browser chrome and web content.
     */
    private View browserContainer;

    private View forwardButton;
    private View backButton;
    private View refreshButton;
    private View stopButton;

    private IWebView.FullscreenCallback fullscreenCallback;

    private boolean isLoading = false;

    private DownloadManager manager;

    private DownloadBroadcastReceiver downloadBroadcastReceiver;

    // Set an initial WeakReference so we never have to handle loadStateListenerWeakReference being null
    // (i.e. so we can always just .get()).
    private WeakReference<LoadStateListener> loadStateListenerWeakReference = new WeakReference<>(null);

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        BrowsingNotificationService.start(context);
    }

    @Override
    public void onPause() {
        super.onPause();
        getContext().unregisterReceiver(downloadBroadcastReceiver);

        final BrowserMenu menu = menuWeakReference.get();
        if (menu != null) {
            menu.dismiss();

            menuWeakReference.clear();
        }
    }

    @Override
    public String getInitialUrl() {
        return getArguments().getString(ARGUMENT_URL);
    }

    private void updateURL(final String url) {
        urlView.setText(UrlUtils.stripUserInfo(url));
    }

    @Override
    public View inflateLayout(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        if (savedInstanceState != null && savedInstanceState.containsKey(RESTORE_KEY_DOWNLOAD)) {
            // If this activity was destroyed before we could start a download (e.g. because we were waiting for a permission)
            // then restore the download object.
            pendingDownload = savedInstanceState.getParcelable(RESTORE_KEY_DOWNLOAD);
        }

        final View view = inflater.inflate(R.layout.fragment_browser, container, false);

        videoContainer = (ViewGroup) view.findViewById(R.id.video_container);
        browserContainer = view.findViewById(R.id.browser_container);

        urlView = (TextView) view.findViewById(R.id.display_url);
        updateURL(getInitialUrl());

        backgroundView = view.findViewById(R.id.background);
        backgroundTransition = (TransitionDrawable) backgroundView.getBackground();

        if ((refreshButton = view.findViewById(R.id.refresh)) != null) {
            refreshButton.setOnClickListener(this);
        }

        if ((stopButton = view.findViewById(R.id.stop)) != null) {
            stopButton.setOnClickListener(this);
        }

        if ((forwardButton = view.findViewById(R.id.forward)) != null) {
            forwardButton.setOnClickListener(this);
        }

        if ((backButton = view.findViewById(R.id.back)) != null) {
            backButton.setOnClickListener(this);
        }

        blockView = (FrameLayout) view.findViewById(R.id.block);

        lockView = (ImageView) view.findViewById(R.id.lock);

        progressView = (AnimatedProgressBar) view.findViewById(R.id.progress);

        menuView = (ImageButton) view.findViewById(R.id.menu);
        menuView.setOnClickListener(this);

        if (BrowsingSession.getInstance().isCustomTab()) {
            initialiseCustomTabUi(view);
        } else {
            initialiseNormalBrowserUi(view);
        }

        return view;
    }

    private void initialiseNormalBrowserUi(final @NonNull View view) {
        final View erase = view.findViewById(R.id.erase);
        erase.setOnClickListener(this);

        urlView.setOnClickListener(this);
    }

    private void initialiseCustomTabUi(final @NonNull View view) {
        final CustomTabConfig customTabConfig = BrowsingSession.getInstance().getCustomTabConfig();

        // Unfortunately there's no simpler way to have the FAB only in normal-browser mode.
        // - ViewStub: requires splitting attributes for the FAB between the ViewStub, and actual FAB layout file.
        //             Moreover, the layout behaviour just doesn't work unless you set it programatically.
        // - View.GONE: doesn't work because the layout-behaviour makes the FAB visible again when scrolling.
        // - Adding at runtime: works, but then we need to use a separate layout file (and you need
        //   to set some attributes programatically, same as ViewStub).
        final View erase = view.findViewById(R.id.erase);
        final ViewGroup eraseContainer = (ViewGroup) erase.getParent();
        eraseContainer.removeView(erase);

        final int textColor;

        final View toolbar = view.findViewById(R.id.urlbar);
        if (customTabConfig.toolbarColor != null) {
            toolbar.setBackgroundColor(customTabConfig.toolbarColor);

            textColor = ColorUtils.getReadableTextColor(customTabConfig.toolbarColor);
            urlView.setTextColor(textColor);
        } else {
            textColor = Color.WHITE;
        }

        final ImageView closeButton = (ImageView) view.findViewById(R.id.customtab_close);

        closeButton.setVisibility(View.VISIBLE);
        closeButton.setOnClickListener(this);

        if (customTabConfig.closeButtonIcon != null) {
            closeButton.setImageBitmap(customTabConfig.closeButtonIcon);
        } else {
            // Always set the icon in case it's been overridden by a previous CT invocation
            final Drawable closeIcon = DrawableUtils.loadAndTintDrawable(getContext(), R.drawable.ic_close, textColor);

            closeButton.setImageDrawable(closeIcon);
        }

        if (customTabConfig.disableUrlbarHiding) {
            AppBarLayout.LayoutParams params = (AppBarLayout.LayoutParams) toolbar.getLayoutParams();
            params.setScrollFlags(0);
        }

        if (customTabConfig.actionButtonConfig != null) {
            final ImageButton actionButton = (ImageButton) view.findViewById(R.id.customtab_actionbutton);
            actionButton.setVisibility(View.VISIBLE);

            actionButton.setImageBitmap(customTabConfig.actionButtonConfig.icon);
            actionButton.setContentDescription(customTabConfig.actionButtonConfig.description);

            final PendingIntent pendingIntent = customTabConfig.actionButtonConfig.pendingIntent;

            actionButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    try {
                        final Intent intent = new Intent();
                        intent.setData(Uri.parse(getUrl()));

                        pendingIntent.send(getContext(), 0, intent);
                    } catch (PendingIntent.CanceledException e) {
                        // There's really nothing we can do here...
                    }
                    TelemetryWrapper.customTabActionButtonEvent();
                }
            });
        }

        // We need to tint some icons.. We already tinted the close button above. Let's tint our other icons too.
        final Drawable lockIcon = DrawableUtils.loadAndTintDrawable(getContext(), R.drawable.ic_lock, textColor);
        lockView.setImageDrawable(lockIcon);

        final Drawable menuIcon = DrawableUtils.loadAndTintDrawable(getContext(), R.drawable.ic_menu, textColor);
        menuView.setImageDrawable(menuIcon);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        if (pendingDownload != null) {
            // We were not able to start this download yet (waiting for a permission). Save this download
            // so that we can start it once we get restored and receive the permission.
            outState.putParcelable(RESTORE_KEY_DOWNLOAD, pendingDownload);
        }
    }

    public interface LoadStateListener {
        void isLoadingChanged(boolean isLoading);
    }

    /**
     * Set a (singular) LoadStateListener. Only one listener is supported at any given time. Setting
     * a new listener means any previously set listeners will be dropped. This is only intended
     * to be used by NavigationItemViewHolder. If you want to use this method for any other
     * parts of the codebase, please extend it to handle a list of listeners. (We would also need
     * to automatically clean up expired listeners from that list, probably when adding to that list.)
     *
     * @param listener The listener to notify of load state changes. Only a weak reference will be kept,
     *                 no more calls will be sent once the listener is garbage collected.
     */
    public void setIsLoadingListener(final LoadStateListener listener) {
        loadStateListenerWeakReference = new WeakReference<>(listener);
    }

    private void updateIsLoading(final boolean isLoading) {
        this.isLoading = isLoading;
        final LoadStateListener currentListener = loadStateListenerWeakReference.get();
        if (currentListener != null) {
            currentListener.isLoadingChanged(isLoading);
        }
    }

    @Override
    public IWebView.Callback createCallback() {
        return new IWebView.Callback() {
            @Override
            public void onPageStarted(final String url) {
                updateIsLoading(true);

                lockView.setVisibility(View.GONE);

                // Hide badging while loading
                updateBlockingBadging(true);

                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading));

                backgroundTransition.resetTransition();

                progressView.setProgress(5);
                progressView.setVisibility(View.VISIBLE);

                updateToolbarButtonStates();
            }

            @Override
            public void onPageFinished(boolean isSecure) {
                updateIsLoading(false);

                backgroundTransition.startTransition(ANIMATION_DURATION);

                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading_finished));

                progressView.setVisibility(View.GONE);

                if (isSecure) {
                    lockView.setVisibility(View.VISIBLE);
                }

                updateBlockingBadging(isBlockingEnabled());

                updateToolbarButtonStates();
            }

            @Override
            public void onURLChanged(final String url) {
                updateURL(url);
            }

            @Override
            public void onProgress(int progress) {
                progressView.setProgress(progress);
            }

            @Override
            public boolean handleExternalUrl(final String url) {
                final IWebView webView = getWebView();

                return webView != null && IntentUtils.handleExternalUri(getContext(), webView, url);
            }

            @Override
            public void onLongPress(final IWebView.HitTarget hitTarget) {
                WebContextMenu.show(getActivity(), this, hitTarget);
            }

            @Override
            public void onEnterFullScreen(@NonNull final IWebView.FullscreenCallback callback, @Nullable View view) {
                fullscreenCallback = callback;

                if (view != null) {
                    // Hide browser UI and web content
                    browserContainer.setVisibility(View.INVISIBLE);

                    // Add view to video container and make it visible
                    final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
                    videoContainer.addView(view, params);
                    videoContainer.setVisibility(View.VISIBLE);

                    // Switch to immersive mode: Hide system bars other UI controls
                    switchToImmersiveMode();
                }
            }

            @Override
            public void onExitFullScreen() {
                // Remove custom video views and hide container
                videoContainer.removeAllViews();
                videoContainer.setVisibility(View.GONE);

                // Show browser UI and web content again
                browserContainer.setVisibility(View.VISIBLE);

                exitImmersiveMode();

                // Notify renderer that we left fullscreen mode.
                if (fullscreenCallback != null) {
                    fullscreenCallback.fullScreenExited();
                    fullscreenCallback = null;
                }
            }

            @Override
            public void onDownloadStart(Download download) {
                if (PackageManager.PERMISSION_GRANTED == ContextCompat.checkSelfPermission(getContext(), Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                    // Long press image displays its own dialog and we handle other download cases here
                    if (!isDownloadFromLongPressImage(download)) {
                        showDownloadPromptDialog(download);
                    } else {
                        // Download dialog has already been shown from long press on image. Proceed with download.
                        queueDownload(download);
                    }
                } else {
                    // We do not have the permission to write to the external storage. Request the permission and start the
                    // download from onRequestPermissionsResult().
                    final Activity activity = getActivity();
                    if (activity == null) {
                        return;
                    }

                    pendingDownload = download;

                    requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, REQUEST_CODE_STORAGE_PERMISSION);
                }
            }
        };
    }

    /**
     * Checks a download's destination directory to determine if it is being called from
     * a long press on an image or otherwise.
     */
    private boolean isDownloadFromLongPressImage(Download download) {
        return download.getDestinationDirectory().equals(Environment.DIRECTORY_PICTURES);
    }

    /**
     * Hide system bars. They can be revealed temporarily with system gestures, such as swiping from
     * the top of the screen. These transient system bars will overlay app’s content, may have some
     * degree of transparency, and will automatically hide after a short timeout.
     */
    private void switchToImmersiveMode() {
        final Activity activity = getActivity();
        if (activity == null) {
            return;
        }

        final Window window = activity.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        window.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    /**
     * Show the system bars again.
     */
    private void exitImmersiveMode() {
        final Activity activity = getActivity();
        if (activity == null) {
            return;
        }

        final Window window = activity.getWindow();
        window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        window.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode != REQUEST_CODE_STORAGE_PERMISSION) {
            return;
        }

        if (grantResults.length <= 0 || grantResults[0] != PackageManager.PERMISSION_GRANTED) {
            // We didn't get the storage permission: We are not able to start this download.
            pendingDownload = null;
        }

        // The actual download dialog will be shown from onResume(). If this activity/fragment is
        // getting restored then we need to 'resume' first before we can show a dialog (attaching
        // another fragment).
    }

    void showDownloadPromptDialog(Download download) {
        final FragmentManager fragmentManager = getFragmentManager();

        if (fragmentManager.findFragmentByTag(DownloadDialogFragment.FRAGMENT_TAG) != null) {
            // We are already displaying a download dialog fragment (Probably a restored fragment).
            // No need to show another one.
            return;
        }

        final DialogFragment downloadDialogFragment = DownloadDialogFragment.newInstance(download);
        downloadDialogFragment.setTargetFragment(BrowserFragment.this, 300);

        try {
            downloadDialogFragment.show(fragmentManager, DownloadDialogFragment.FRAGMENT_TAG);
        } catch (IllegalStateException e) {
            // It can happen that at this point in time the activity is already in the background
            // and onSaveInstanceState() has already been called. Fragment transactions are not
            // allowed after that anymore. It's probably safe to guess that the user might not
            // be interested in the download at this point. So we could just *not* show the dialog.
            // Unfortunately we can't call commitAllowingStateLoss() because committing the
            // transaction is happening inside the DialogFragment code. Therefore we just swallow
            // the exception here. Gulp!
        }
    }

    @Override
    public void onFinishDownloadDialog(Download download, boolean shouldDownload) {
        if (shouldDownload) {
            queueDownload(download);
        }
    }

    @Override
    public void onCreateViewCalled() {
        manager = (DownloadManager) getContext().getSystemService(Context.DOWNLOAD_SERVICE);
        downloadBroadcastReceiver = new DownloadBroadcastReceiver(browserContainer, manager);
    }

    @Override
    public void onResume() {
        super.onResume();

        final IntentFilter filter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
        getContext().registerReceiver(downloadBroadcastReceiver, filter);

        if (pendingDownload != null
                && PackageManager.PERMISSION_GRANTED == ContextCompat.checkSelfPermission(getContext(), Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
            // There's a pending download (waiting for the storage permission) and now we have the
            // missing permission: Show the dialog to ask whether the user wants to actually proceed
            // with downloading this file.
            showDownloadPromptDialog(pendingDownload);
            pendingDownload = null;
        }
    }

    /**
     * Use Android's Download Manager to queue this download.
     */
    private void queueDownload(Download download) {
        if (download == null) {
            return;
        }

        final Context context = getContext();
        if (context == null) {
            return;
        }

        final String cookie = CookieManager.getInstance().getCookie(download.getUrl());
        final String fileName = URLUtil.guessFileName(
                download.getUrl(), download.getContentDisposition(), download.getMimeType());

        final DownloadManager.Request request = new DownloadManager.Request(Uri.parse(download.getUrl()))
                .addRequestHeader("User-Agent", download.getUserAgent())
                .addRequestHeader("Cookie", cookie)
                .addRequestHeader("Referer", getUrl())
                .setDestinationInExternalPublicDir(download.getDestinationDirectory(), fileName)
                .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
                .setMimeType(download.getMimeType());

        request.allowScanningByMediaScanner();

        long downloadReference = manager.enqueue(request);
        downloadBroadcastReceiver.addQueuedDownload(downloadReference);
    }

    private boolean isStartedFromExternalApp() {
        final Activity activity = getActivity();
        if (activity == null) {
            return false;
        }

        // No SafeIntent needed here because intent.getAction() is safe (SafeIntent simply calls intent.getAction()
        // without any wrapping):
        final Intent intent = activity.getIntent();
        return intent != null && Intent.ACTION_VIEW.equals(intent.getAction());
    }

    public boolean onBackPressed() {
        if (canGoBack()) {
            // Go back in web history
            goBack();
        } else {
            if (isStartedFromExternalApp()) {
                // We have been started from a VIEW intent. Go back to the previous app immediately
                // and erase the current browsing session.
                erase();

                // We remove the whole task because otherwise the old session might still be
                // partially visible in the app switcher.
                getActivity().finishAndRemoveTask();

                // We can't show a snackbar outside of the app. So let's show a toast instead.
                Toast.makeText(getContext(), R.string.feedback_erase, Toast.LENGTH_SHORT).show();

                TelemetryWrapper.eraseBackToAppEvent();
            } else {
                // Just go back to the home screen.
                eraseAndShowHomeScreen(true);

                TelemetryWrapper.eraseBackToHomeEvent();
            }
        }

        return true;
    }

    public void erase() {
        final IWebView webView = getWebView();
        if (webView != null) {
            webView.cleanup();
        }

        BrowsingNotificationService.stop(getContext());
    }

    public void eraseAndShowHomeScreen(final boolean animateErase) {
        erase();

        final FragmentTransaction transaction = getActivity().getSupportFragmentManager()
                .beginTransaction();

        if (animateErase) {
            transaction.setCustomAnimations(0, R.anim.erase_animation);
        }

        transaction
                .replace(R.id.container, UrlInputFragment.createWithBackground(), UrlInputFragment.FRAGMENT_TAG)
                .commit();

        ViewUtils.showBrandedSnackbar(getActivity().findViewById(android.R.id.content),
                R.string.feedback_erase,
                getResources().getInteger(R.integer.erase_snackbar_delay));
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.menu:
                final CustomTabConfig customTabConfig;
                if (BrowsingSession.getInstance().isCustomTab()) {
                    customTabConfig = BrowsingSession.getInstance().getCustomTabConfig();
                } else {
                    customTabConfig = null;
                }

                BrowserMenu menu = new BrowserMenu(getActivity(), this, customTabConfig);
                menu.show(menuView);

                menuWeakReference = new WeakReference<>(menu);
                break;

            case R.id.display_url:
                final Fragment urlFragment = UrlInputFragment
                        .createAsOverlay(UrlUtils.getSearchTermsOrUrl(getContext(), getUrl()), urlView);

                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .add(R.id.container, urlFragment, UrlInputFragment.FRAGMENT_TAG)
                        .commit();
                break;

            case R.id.erase: {
                eraseAndShowHomeScreen(true);

                TelemetryWrapper.eraseEvent();
                break;
            }

            case R.id.back: {
                goBack();
                break;
            }

            case R.id.forward: {
                final IWebView webView = getWebView();
                if (webView != null) {
                    webView.goForward();
                }
                break;
            }

            case R.id.refresh: {
                reload();
                break;
            }

            case R.id.stop: {
                final IWebView webView = getWebView();
                if (webView != null) {
                    webView.stopLoading();
                }
                break;
            }

            case R.id.share: {
                final Intent shareIntent = new Intent(Intent.ACTION_SEND);
                shareIntent.setType("text/plain");
                shareIntent.putExtra(Intent.EXTRA_TEXT, getUrl());
                startActivity(Intent.createChooser(shareIntent, getString(R.string.share_dialog_title)));

                TelemetryWrapper.shareEvent();
                break;
            }

            case R.id.settings:
                ((LocaleAwareAppCompatActivity) getActivity()).openPreferences();
                break;

            case R.id.open_default: {
                final Browsers browsers = new Browsers(getContext(), getUrl());

                final ActivityInfo defaultBrowser = browsers.getDefaultBrowser();

                if (defaultBrowser == null) {
                    // We only add this menu item when a third party default exists, in
                    // BrowserMenuAdapter.initializeMenu()
                    throw new IllegalStateException("<Open with $Default> was shown when no default browser is set");
                }

                final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(getUrl()));
                intent.setPackage(defaultBrowser.packageName);
                startActivity(intent);

                TelemetryWrapper.openDefaultAppEvent();
                break;
            }

            case R.id.open_firefox: {
                final Browsers browsers = new Browsers(getContext(), getUrl());

                if (browsers.hasFirefoxBrandedBrowserInstalled()) {
                    final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(getUrl()));
                    intent.setPackage(browsers.getFirefoxBrandedBrowser().packageName);
                    startActivity(intent);
                } else {
                    InstallFirefoxActivity.open(getContext());
                }

                TelemetryWrapper.openFirefoxEvent();
                break;
            }

            case R.id.open_select_browser: {
                final Browsers browsers = new Browsers(getContext(), getUrl());

                final OpenWithFragment fragment = OpenWithFragment.newInstance(
                        browsers.getInstalledBrowsers(), getUrl());
                fragment.show(getFragmentManager(), OpenWithFragment.FRAGMENT_TAG);

                TelemetryWrapper.openSelectionEvent();
                break;
            }

            case R.id.customtab_close: {
                erase();
                getActivity().finish();

                TelemetryWrapper.closeCustomTabEvent();
                break;
            }

            case R.id.help:
                Intent helpIntent = InfoActivity.getHelpIntent(getActivity());
                startActivity(helpIntent);
                break;

            case R.id.help_trackers:
                Intent trackerHelpIntent = InfoActivity.getTrackerHelpIntent(getActivity());
                startActivity(trackerHelpIntent);
                break;

            case R.id.add_to_homescreen:
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

                final Bitmap icon = webView.getIcon();
                final String url = webView.getUrl();
                final String title = webView.getTitle();

                HomeScreen.installShortCut(getContext(), icon, url, title);
                break;

            default:
                throw new IllegalArgumentException("Unhandled menu item in BrowserFragment");
        }
    }

    private void updateToolbarButtonStates() {
        if (forwardButton == null || backButton == null || refreshButton == null || stopButton == null) {
            return;
        }

        final IWebView webView = getWebView();
        if (webView == null) {
            return;
        }

        final boolean canGoForward = webView.canGoForward();
        final boolean canGoBack = webView.canGoBack();

        forwardButton.setEnabled(canGoForward);
        forwardButton.setAlpha(canGoForward ? 1.0f : 0.5f);
        backButton.setEnabled(canGoBack);
        backButton.setAlpha(canGoBack ? 1.0f : 0.5f);

        refreshButton.setVisibility(isLoading ? View.GONE : View.VISIBLE);
        stopButton.setVisibility(isLoading ? View.VISIBLE : View.GONE);
    }

    @NonNull
    public String getUrl() {
        // getUrl() is used for things like sharing the current URL. We could try to use the webview,
        // but sometimes it's null, and sometimes it returns a null URL. Sometimes it returns a data:
        // URL for error pages. The URL we show in the toolbar is (A) always correct and (B) what the
        // user is probably expecting to share, so lets use that here:
        return urlView.getText().toString();
    }

    public boolean canGoForward() {
        final IWebView webView = getWebView();
        return webView != null && webView.canGoForward();
    }

    public boolean isLoading() {
        return isLoading;
    }

    public boolean canGoBack() {
        final IWebView webView = getWebView();
        return webView != null && webView.canGoBack();
    }

    public void goBack() {
        final IWebView webView = getWebView();
        if (webView != null) {
            webView.goBack();
        }
    }

    public void loadUrl(final String url) {
        final IWebView webView = getWebView();
        if (webView != null) {
            webView.loadUrl(url);
        }
    }

    public void reload() {
        final IWebView webView = getWebView();
        if (webView != null) {
            webView.reload();
        }
    }

    public void setBlockingEnabled(boolean enabled) {
        final IWebView webView = getWebView();
        if (webView != null) {
            webView.setBlockingEnabled(enabled);
        }
        backgroundView.setBackgroundResource(enabled ? R.drawable.animated_background : R.drawable.animated_background_disabled);
        backgroundTransition = (TransitionDrawable) backgroundView.getBackground();
    }

    public boolean isBlockingEnabled() {
        final IWebView webView = getWebView();
        return webView == null || webView.isBlockingEnabled();
    }

    // In the future, if more badging icons are needed, this should be abstracted
    public void updateBlockingBadging(boolean enabled) {
        blockView.setVisibility(enabled ? View.GONE : View.VISIBLE);
    }
}
