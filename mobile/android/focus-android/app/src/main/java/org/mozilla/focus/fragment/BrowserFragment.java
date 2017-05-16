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
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.drawable.TransitionDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.design.widget.AppBarLayout;
import android.support.v4.app.Fragment;
import android.support.v4.content.ContextCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.URLUtil;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InstallFirefoxActivity;
import org.mozilla.focus.activity.SettingsActivity;
import org.mozilla.focus.menu.BrowserMenu;
import org.mozilla.focus.menu.WebContextMenu;
import org.mozilla.focus.notification.BrowsingNotificationService;
import org.mozilla.focus.open.OpenWithFragment;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.AppConstants;
import org.mozilla.focus.utils.Browsers;
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
public class BrowserFragment extends WebFragment implements View.OnClickListener {
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
    private TransitionDrawable backgroundTransition;
    private TextView urlView;
    private AnimatedProgressBar progressView;
    private View lockView;
    private View menuView;

    private View forwardButton;
    private View backButton;
    private View refreshButton;
    private View stopButton;

    private boolean isLoading = false;

    // Set an initial WeakReference so we never have to handle loadStateListenerWeakReference being null
    // (i.e. so we can always just .get()).
    private WeakReference<LoadStateListener> loadStateListenerWeakReference = new WeakReference<>(null);

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        BrowsingNotificationService.start(context);
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

        urlView = (TextView) view.findViewById(R.id.display_url);
        updateURL(getInitialUrl());

        backgroundTransition = (TransitionDrawable) view.findViewById(R.id.background).getBackground();

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

        lockView = view.findViewById(R.id.lock);

        progressView = (AnimatedProgressBar) view.findViewById(R.id.progress);

        menuView = view.findViewById(R.id.menu);
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
        if (customTabConfig == null) {
            throw new IllegalStateException("Can't initialise custom tab UI for non custom-tab session");
        }

        // Unfortunately there's no simpler way to have the FAB only in normal-browser mode.
        // - ViewStub: requires splitting attributes for the FAB between the ViewStub, and actual FAB layout file.
        //             Moreover, the layout behaviour just doesn't work unless you set it programatically.
        // - View.GONE: doesn't work because the layout-behaviour makes the FAB visible again when scrolling.
        // - Adding at runtime: works, but then we need to use a separate layout file (and you need
        //   to set some attributes programatically, same as ViewStub).
        final View erase = view.findViewById(R.id.erase);
        final ViewGroup eraseContainer = (ViewGroup) erase.getParent();
        eraseContainer.removeView(erase);

        final View toolbar = view.findViewById(R.id.urlbar);
        if (customTabConfig.toolbarColor != null) {
            toolbar.setBackgroundColor(customTabConfig.toolbarColor);
        }

        final ImageView closeButton = (ImageView) view.findViewById(R.id.customtab_close);

        closeButton.setVisibility(View.VISIBLE);
        closeButton.setOnClickListener(this);

        if (customTabConfig.closeButtonIcon != null) {
            closeButton.setImageBitmap(customTabConfig.closeButtonIcon);
        } else {
            // Always set the icon in case it's been overridden by a previous CT invocation
            closeButton.setImageResource(R.drawable.ic_close);
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
                        pendingIntent.send();
                    } catch (PendingIntent.CanceledException e) {
                        // There's really nothing we can do here...
                    }
                }
            });
        }
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

                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading));

                backgroundTransition.resetTransition();

                progressView.setVisibility(View.VISIBLE);

                updateToolbarButtonStates();
            }

            @Override
            public void onPageFinished(boolean isSecure) {
                updateIsLoading(false);

                if (isBlockingEnabled()) {
                    // We only show the colorful background gradient when content blocking is enabled.
                    backgroundTransition.startTransition(ANIMATION_DURATION);
                }

                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading_finished));

                progressView.setVisibility(View.GONE);

                if (isSecure) {
                    lockView.setVisibility(View.VISIBLE);
                }

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
            public void onDownloadStart(Download download) {
                if (PackageManager.PERMISSION_GRANTED == ContextCompat.checkSelfPermission(getContext(), Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                    // We do have the permission to write to the external storage. Proceed with the download.
                    queueDownload(download);
                } else {
                    // We do not have the permission to write to the external storage. Request the permission and start the
                    // download from onRequestPermissionsResult().
                    final Activity activity = getActivity();
                    if (activity == null) {
                        return;
                    }

                    pendingDownload = download;

                    requestPermissions(new String[] { Manifest.permission.WRITE_EXTERNAL_STORAGE }, REQUEST_CODE_STORAGE_PERMISSION);
                }
            }
        };
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode != REQUEST_CODE_STORAGE_PERMISSION) {
            return;
        }

        if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            queueDownload(pendingDownload);
        }

        pendingDownload = null;
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

        final DownloadManager manager = (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
        manager.enqueue(request);
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
                // We have been started from a VIEW intent. Go back to the previous app immediately (No erase).
                getActivity().finish();
            } else {
                // Just go back to the home screen.
                eraseAndShowHomeScreen();
            }

            TelemetryWrapper.eraseBackEvent();
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

    public void eraseAndShowHomeScreen() {
        erase();

        getActivity().getSupportFragmentManager()
                .beginTransaction()
                .setCustomAnimations(0, R.anim.erase_animation)
                .replace(R.id.container, HomeFragment.create(), HomeFragment.FRAGMENT_TAG)
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
                break;

            case R.id.display_url:
                final Fragment urlFragment = UrlInputFragment.createWithBrowserScreenAnimation(getUrl(), urlView);

                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .add(R.id.container, urlFragment, UrlInputFragment.FRAGMENT_TAG)
                        .commit();
                break;

            case R.id.erase: {
                eraseAndShowHomeScreen();

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
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

                final Intent shareIntent = new Intent(Intent.ACTION_SEND);
                shareIntent.setType("text/plain");
                shareIntent.putExtra(Intent.EXTRA_TEXT, webView.getUrl());
                startActivity(Intent.createChooser(shareIntent, getString(R.string.share_dialog_title)));

                TelemetryWrapper.shareEvent();
                break;
            }

            case R.id.settings:
                final Intent settingsIntent = new Intent(getActivity(), SettingsActivity.class);
                startActivity(settingsIntent);
                break;

            case R.id.open_default: {
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

                final Browsers browsers = new Browsers(getContext(), webView.getUrl());

                final ActivityInfo defaultBrowser = browsers.getDefaultBrowser();

                if (defaultBrowser == null) {
                    // We only add this menu item when a third party default exists, in
                    // BrowserMenuAdapter.initializeMenu()
                    throw new IllegalStateException("<Open with $Default> was shown when no default browser is set");
                }

                final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(webView.getUrl()));
                intent.setPackage(defaultBrowser.packageName);
                startActivity(intent);

                TelemetryWrapper.openDefaultAppEvent();
                break;
            }

            case R.id.open_firefox: {
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

                final Browsers browsers = new Browsers(getContext(), webView.getUrl());

                if (browsers.hasFirefoxBrandedBrowserInstalled()) {
                    final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(webView.getUrl()));
                    intent.setPackage(browsers.getFirefoxBrandedBrowser().packageName);
                    startActivity(intent);
                } else {
                    InstallFirefoxActivity.open(getContext());
                }

                TelemetryWrapper.openFirefoxEvent();
                break;
            }

            case R.id.open_select_browser: {
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

                final Browsers browsers = new Browsers(getContext(), webView.getUrl());

                final OpenWithFragment fragment = OpenWithFragment.newInstance(
                        browsers.getInstalledBrowsers(), webView.getUrl());
                fragment.show(getFragmentManager(), OpenWithFragment.FRAGMENT_TAG);

                TelemetryWrapper.openSelectionEvent();
                break;
            }

            case R.id.customtab_close: {
                erase();
                getActivity().finish();
                break;
            }

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

    @Nullable
    public String getUrl() {
        final IWebView webView = getWebView();
        return webView != null ? webView.getUrl() : null;
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
    }

    public boolean isBlockingEnabled() {
        final IWebView webView = getWebView();
        return webView == null || webView.isBlockingEnabled();
    }
}
