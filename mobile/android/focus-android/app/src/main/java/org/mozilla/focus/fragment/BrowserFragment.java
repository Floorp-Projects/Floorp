/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.Manifest;
import android.app.Activity;
import android.app.DownloadManager;
import android.app.PendingIntent;
import android.arch.lifecycle.Observer;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.design.widget.AppBarLayout;
import android.support.design.widget.CoordinatorLayout;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.content.ContextCompat;
import android.support.v4.widget.SwipeRefreshLayout;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.webkit.CookieManager;
import android.webkit.URLUtil;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;
import android.widget.Toast;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InstallFirefoxActivity;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.animation.TransitionDrawableGroup;
import org.mozilla.focus.architecture.NonNullObserver;
import org.mozilla.focus.autocomplete.AutocompleteQuickAddPopup;
import org.mozilla.focus.broadcastreceiver.DownloadBroadcastReceiver;
import org.mozilla.focus.customtabs.CustomTabConfig;
import org.mozilla.focus.findinpage.FindInPageCoordinator;
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity;
import org.mozilla.focus.menu.browser.BrowserMenu;
import org.mozilla.focus.menu.context.WebContextMenu;
import org.mozilla.focus.observer.AverageLoadTimeObserver;
import org.mozilla.focus.open.OpenWithFragment;
import org.mozilla.focus.popup.PopupUtils;
import org.mozilla.focus.session.NullSession;
import org.mozilla.focus.session.Session;
import org.mozilla.focus.session.SessionCallbackProxy;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.session.Source;
import org.mozilla.focus.session.ui.SessionsSheetFragment;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.Browsers;
import org.mozilla.focus.utils.Features;
import org.mozilla.focus.utils.StatusBarUtils;
import org.mozilla.focus.utils.SupportUtils;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.utils.ViewUtils;
import org.mozilla.focus.web.Download;
import org.mozilla.focus.web.IWebView;
import org.mozilla.focus.web.HttpAuthenticationDialogBuilder;
import org.mozilla.focus.widget.AnimatedProgressBar;
import org.mozilla.focus.widget.FloatingEraseButton;
import org.mozilla.focus.widget.FloatingSessionsButton;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.Objects;

import kotlin.Unit;
import kotlin.jvm.functions.Function1;
import mozilla.components.support.utils.ColorUtils;
import mozilla.components.support.utils.DownloadUtils;
import mozilla.components.support.utils.DrawableUtils;

/**
 * Fragment for displaying the browser UI.
 */
@SuppressWarnings({"PMD.ExcessiveClassLength", "PMD.CyclomaticComplexity", "PMD.TooManyMethods",
        "PMD.ModifiedCyclomaticComplexity", "PMD.TooManyFields", "PMD.StdCyclomaticComplexity" })
public class BrowserFragment extends WebFragment implements View.OnClickListener, DownloadDialogFragment.DownloadDialogListener, View.OnLongClickListener {
    public static final String FRAGMENT_TAG = "browser";

    private static final int REQUEST_CODE_STORAGE_PERMISSION = 101;
    private static final int ANIMATION_DURATION = 300;

    private static final String ARGUMENT_SESSION_UUID = "sessionUUID";
    private static final String RESTORE_KEY_DOWNLOAD = "download";

    public static BrowserFragment createForSession(Session session) {
        final Bundle arguments = new Bundle();
        arguments.putString(ARGUMENT_SESSION_UUID, session.getUUID());

        BrowserFragment fragment = new BrowserFragment();
        fragment.setArguments(arguments);

        return fragment;
    }

    private Download pendingDownload;
    private TransitionDrawableGroup backgroundTransitionGroup;
    private TextView urlView;
    private AnimatedProgressBar progressView;
    private FrameLayout blockView;
    private ImageView securityView;
    private ImageButton menuView;
    private View statusBar;
    private View urlBar;
    private FrameLayout popupTint;
    private SwipeRefreshLayout swipeRefresh;
    private WeakReference<BrowserMenu> menuWeakReference = new WeakReference<>(null);
    private WeakReference<AutocompleteQuickAddPopup> autocompletePopupWeakReference = new WeakReference<>(null);

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

    private View findInPageView;
    private int findInPageViewHeight;
    private TextView findInPageQuery;
    private TextView findInPageResultTextView;
    private ImageButton findInPageNext;
    private ImageButton findInPagePrevious;
    private ImageButton closeFindInPage;

    private IWebView.FullscreenCallback fullscreenCallback;

    private DownloadManager manager;

    private DownloadBroadcastReceiver downloadBroadcastReceiver;

    private final SessionManager sessionManager;
    private Session session;
    private final FindInPageCoordinator findInPageCoordinator = new FindInPageCoordinator();

    public BrowserFragment() {
        sessionManager = SessionManager.getInstance();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final String sessionUUID = getArguments().getString(ARGUMENT_SESSION_UUID);
        if (sessionUUID == null) {
            throw new IllegalAccessError("No session exists");
        }

        session = sessionManager.hasSessionWithUUID(sessionUUID)
                ? sessionManager.getSessionByUUID(sessionUUID)
                : new NullSession();

        session.getBlockedTrackers().observe(this, new Observer<Integer>() {
            @Override
            public void onChanged(@Nullable Integer blockedTrackers) {
                if (menuWeakReference == null) {
                    return;
                }

                final BrowserMenu menu = menuWeakReference.get();

                if (menu != null) {
                    //noinspection ConstantConditions - Not null
                    menu.updateTrackers(blockedTrackers);
                }
            }
        });

        findInPageCoordinator.getMatches().observe(this, new Observer<kotlin.Pair<Integer, Integer>>() {
            @Override
            public void onChanged(@Nullable kotlin.Pair<Integer, Integer> matches) {
                updateFindInPageResult(matches.getFirst(), matches.getSecond());
            }
        });
    }

    public Session getSession() {
        return session;
    }

    @Override
    public String getInitialUrl() {
        return session.getUrl().getValue();
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
    public View inflateLayout(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        if (savedInstanceState != null && savedInstanceState.containsKey(RESTORE_KEY_DOWNLOAD)) {
            // If this activity was destroyed before we could start a download (e.g. because we were waiting for a permission)
            // then restore the download object.
            pendingDownload = savedInstanceState.getParcelable(RESTORE_KEY_DOWNLOAD);
        }

        final View view = inflater.inflate(R.layout.fragment_browser, container, false);

        videoContainer = (ViewGroup) view.findViewById(R.id.video_container);
        browserContainer = view.findViewById(R.id.browser_container);

        urlBar = view.findViewById(R.id.urlbar);
        statusBar = view.findViewById(R.id.status_bar_background);

        popupTint = view.findViewById(R.id.popup_tint);

        urlView = (TextView) view.findViewById(R.id.display_url);
        urlView.setOnLongClickListener(this);

        progressView = (AnimatedProgressBar) view.findViewById(R.id.progress);

        swipeRefresh = (SwipeRefreshLayout) view.findViewById(R.id.swipe_refresh);
        swipeRefresh.setColorSchemeResources(R.color.colorAccent);
        swipeRefresh.setEnabled(Features.SWIPE_TO_REFRESH);

        swipeRefresh.setOnRefreshListener(new SwipeRefreshLayout.OnRefreshListener() {
            @Override
            public void onRefresh() {
                reload();

                TelemetryWrapper.swipeReloadEvent();
            }
        });

        session.getUrl().observe(this, new Observer<String>() {
            @Override
            public void onChanged(@Nullable String url) {
                urlView.setText(UrlUtils.stripUserInfo(url));
            }
        });

        findInPageView = view.findViewById(R.id.find_in_page);

        findInPageQuery = view.findViewById(R.id.queryText);
        findInPageResultTextView = view.findViewById(R.id.resultText);

        findInPageQuery.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

                    @Override
                    public void afterTextChanged(Editable s) {}

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        final IWebView webView = getWebView();
                        if (webView == null) {
                            return;
                        }

                        webView.findAllAsync(s.toString());
                    }
                }
        );
        findInPageQuery.setOnClickListener(this);
        findInPageQuery.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView textView, int actionId, KeyEvent keyEvent) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    ViewUtils.hideKeyboard(findInPageQuery);
                    findInPageQuery.setCursorVisible(false);
                }
                return false;
            }
        });

        findInPagePrevious = view.findViewById(R.id.previousResult);
        findInPagePrevious.setOnClickListener(this);

        findInPageNext = view.findViewById(R.id.nextResult);
        findInPageNext.setOnClickListener(this);

        closeFindInPage = view.findViewById(R.id.close_find_in_page);
        closeFindInPage.setOnClickListener(this);

        setBlockingEnabled(session.isBlockingEnabled());
        setShouldRequestDesktop(session.shouldRequestDesktopSite());

        session.getLoading().observe(this, new AverageLoadTimeObserver(session));

        session.getLoading().observe(this, new NonNullObserver<Boolean>() {
            @Override
            public void onValueChanged(@NonNull Boolean loading) {
                if (loading) {
                    backgroundTransitionGroup.resetTransition();

                    progressView.setProgress(5);
                    progressView.setVisibility(View.VISIBLE);
                } else {
                    if (progressView.getVisibility() == View.VISIBLE) {
                        // We start a transition only if a page was just loading before
                        // allowing to avoid issue #1179
                        backgroundTransitionGroup.startTransition(ANIMATION_DURATION);
                        progressView.setVisibility(View.GONE);
                    }
                    swipeRefresh.setRefreshing(false);
                }

                updateBlockingBadging(loading || session.isBlockingEnabled());

                updateToolbarButtonStates(loading);

                final BrowserMenu menu = menuWeakReference.get();
                if (menu != null) {
                    menu.updateLoading(loading);
                }

                hideFindInPage();
            }
        });

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

        final ImageView blockIcon = (ImageView) view.findViewById(R.id.block_image);
        blockIcon.setImageResource(R.drawable.ic_tracking_protection_disabled);

        blockView = (FrameLayout) view.findViewById(R.id.block);

        securityView = view.findViewById(R.id.security_info);
        session.getSecure().observe(this, new Observer<Boolean>() {
            @Override
            public void onChanged(Boolean secure) {
                if (!session.getLoading().getValue()) {
                    if (secure) {
                        securityView.setImageResource(R.drawable.ic_lock);
                    } else {
                        if (URLUtil.isHttpUrl(getUrl())) {
                            // HTTP site
                            securityView.setImageResource(R.drawable.ic_internet);
                        } else {
                            // Certificate is bad
                            securityView.setImageResource(R.drawable.ic_warning);
                        }
                    }
                } else {
                    securityView.setImageResource(R.drawable.ic_internet);
                }
            }
        });
        securityView.setOnClickListener(this);

        session.getProgress().observe(this, new Observer<Integer>() {
            @Override
            public void onChanged(Integer progress) {
                progressView.setProgress(progress);
            }
        });

        menuView = (ImageButton) view.findViewById(R.id.menuView);
        menuView.setOnClickListener(this);

        if (session.isCustomTab()) {
            initialiseCustomTabUi(view);
        } else {
            initialiseNormalBrowserUi(view);
        }

        // Pre-calculate the height of the find in page UI so that we can accurately add padding
        // to the WebView when we present it.
        findInPageView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        findInPageViewHeight = findInPageView.getMeasuredHeight();

        return view;
    }

    private void initialiseNormalBrowserUi(final @NonNull View view) {
        final FloatingEraseButton eraseButton = view.findViewById(R.id.erase);
        eraseButton.setOnClickListener(this);

        urlView.setOnClickListener(this);

        final FloatingSessionsButton tabsButton = view.findViewById(R.id.tabs);
        tabsButton.setOnClickListener(this);

        sessionManager.getSessions().observe(this, new NonNullObserver<List<Session>>() {
            @Override
            protected void onValueChanged(@NonNull List<Session> sessions) {
                tabsButton.updateSessionsCount(sessions.size());
                eraseButton.updateSessionsCount(sessions.size());
            }
        });
    }

    private void initialiseCustomTabUi(final @NonNull View view) {
        final CustomTabConfig customTabConfig = session.getCustomTabConfig();

        // Unfortunately there's no simpler way to have the FAB only in normal-browser mode.
        // - ViewStub: requires splitting attributes for the FAB between the ViewStub, and actual FAB layout file.
        //             Moreover, the layout behaviour just doesn't work unless you set it programatically.
        // - View.GONE: doesn't work because the layout-behaviour makes the FAB visible again when scrolling.
        // - Adding at runtime: works, but then we need to use a separate layout file (and you need
        //   to set some attributes programatically, same as ViewStub).
        final FloatingEraseButton erase = view.findViewById(R.id.erase);
        final ViewGroup eraseContainer = (ViewGroup) erase.getParent();
        eraseContainer.removeView(erase);

        final FloatingSessionsButton sessions = view.findViewById(R.id.tabs);
        eraseContainer.removeView(sessions);

        final int textColor;

        if (customTabConfig.toolbarColor != null) {
            urlBar.setBackgroundColor(customTabConfig.toolbarColor);

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
            final Drawable closeIcon = DrawableUtils.INSTANCE.loadAndTintDrawable(getContext(), R.drawable.ic_close, textColor);

            closeButton.setImageDrawable(closeIcon);
        }

        if (customTabConfig.disableUrlbarHiding) {
            AppBarLayout.LayoutParams params = (AppBarLayout.LayoutParams) urlBar.getLayoutParams();
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
        } else {
            // If the third-party app doesn't provide an action button configuration then we are
            // going to disable a "Share" button in the toolbar instead.

            final ImageButton shareButton = view.findViewById(R.id.customtab_actionbutton);
            shareButton.setVisibility(View.VISIBLE);
            shareButton.setImageDrawable(DrawableUtils.INSTANCE.loadAndTintDrawable(getContext(), R.drawable.ic_share, textColor));
            shareButton.setContentDescription(getString(R.string.menu_share));
            shareButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    shareCurrentUrl();
                }
            });
        }

        // We need to tint some icons.. We already tinted the close button above. Let's tint our other icons too.
        securityView.setColorFilter(textColor);

        final Drawable menuIcon = DrawableUtils.INSTANCE.loadAndTintDrawable(getContext(), R.drawable.ic_menu, textColor);
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

    @Override
    public IWebView.Callback createCallback() {
        return new SessionCallbackProxy(session, new IWebView.Callback() {
            @Override
            public void onPageStarted(final String url) {}

            @Override
            public void onPageFinished(boolean isSecure) {}

            @Override
            public void onSecurityChanged(boolean isSecure, String host, String organization) {}

            @Override
            public void onURLChanged(final String url) {}

            @Override
            public void onTitleChanged(String title) {}

            @Override
            public void onRequest(boolean isTriggeredByUserGesture) {}

            @Override
            public void onProgress(int progress) {}

            @Override
            public void countBlockedTracker() {}

            @Override
            public void resetBlockedTrackers() {}

            @Override
            public void onBlockingStateChanged(boolean isBlockingEnabled) {}

            @Override
            public void onHttpAuthRequest(@NonNull final IWebView.HttpAuthCallback callback, String host, String realm) {
                HttpAuthenticationDialogBuilder builder = new HttpAuthenticationDialogBuilder.Builder(getActivity(), host, realm)
                                .setOkListener(new HttpAuthenticationDialogBuilder.OkListener() {
                                    @Override
                                    public void onOk(String host, String realm, String username, String password) {
                                        callback.proceed(username, password);
                                    }
                                })
                                .setCancelListener(new HttpAuthenticationDialogBuilder.CancelListener() {
                                    @Override
                                    public void onCancel() {
                                        callback.cancel();
                                    }
                                })
                                .build();

                builder.createDialog();
                builder.show();
            }

            @Override
            public void onRequestDesktopStateChanged(boolean shouldRequestDesktop) {}

            @Override
            public void onLongPress(final IWebView.HitTarget hitTarget) {
                WebContextMenu.show(getActivity(), this, hitTarget);
            }

            @Override
            public void onEnterFullScreen(@NonNull final IWebView.FullscreenCallback callback, @Nullable View view) {
                fullscreenCallback = callback;

                // View is passed in as null for GeckoView fullscreen
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
                } else {
                    // Hide status bar when entering fullscreen with GeckoView
                    statusBar.setVisibility(View.GONE);
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

                // Show status bar again (hidden in GeckoView versions)
                statusBar.setVisibility(View.VISIBLE);

                exitImmersiveModeIfNeeded();

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
        });
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
    private void exitImmersiveModeIfNeeded() {
        final Activity activity = getActivity();
        if (activity == null) {
            return;
        }

        if ((WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON & activity.getWindow().getAttributes().flags) == 0) {
            // We left immersive mode already.
            return;
        }

        final Window window = activity.getWindow();
        window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        window.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        // This fragment might get destroyed before the user left immersive mode (e.g. by opening another URL from an app).
        // In this case let's leave immersive mode now when the fragment gets destroyed.
        exitImmersiveModeIfNeeded();
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

    void showAddToHomescreenDialog(String url, String title) {
        final FragmentManager fragmentManager = getFragmentManager();

        if (fragmentManager.findFragmentByTag(AddToHomescreenDialogFragment.FRAGMENT_TAG) != null) {
            // We are already displaying a homescreen dialog fragment (Probably a restored fragment).
            // No need to show another one.
            return;
        }

        final AddToHomescreenDialogFragment addToHomescreenDialogFragment = AddToHomescreenDialogFragment.newInstance(url, title, session.isBlockingEnabled(), session.shouldRequestDesktopSite());
        addToHomescreenDialogFragment.setTargetFragment(BrowserFragment.this, 300);

        try {
            addToHomescreenDialogFragment.show(fragmentManager, AddToHomescreenDialogFragment.FRAGMENT_TAG);
        } catch (IllegalStateException e) {
            // It can happen that at this point in time the activity is already in the background
            // and onSaveInstanceState() has already been called. Fragment transactions are not
            // allowed after that anymore. It's probably safe to guess that the user might not
            // be interested in adding to homescreen now.
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

        final IWebView webView = getWebView();
        if (webView != null) {
            webView.setFindListener(findInPageCoordinator);
        }
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

        StatusBarUtils.getStatusBarHeight(statusBar, new StatusBarUtils.StatusBarHeightListener() {
            @Override
            public void onStatusBarHeightFetched(int statusBarHeight) {
                statusBar.getLayoutParams().height = statusBarHeight;
            }
        });
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
        final String fileName = DownloadUtils.guessFileName(
                download.getContentDisposition(),
                download.getUrl(),
                download.getMimeType());

        final DownloadManager.Request request = new DownloadManager.Request(Uri.parse(download.getUrl()))
                .addRequestHeader("User-Agent", download.getUserAgent())
                .addRequestHeader("Cookie", cookie)
                .addRequestHeader("Referer", getUrl())
                .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
                .setMimeType(download.getMimeType());

        try {
            request.setDestinationInExternalPublicDir(
                    download.getDestinationDirectory(), fileName);
        } catch (IllegalStateException e) {
            Log.e(FRAGMENT_TAG, "Cannot create download directory");
            return;
        }

        request.allowScanningByMediaScanner();

        try {
            long downloadReference = manager.enqueue(request);
            downloadBroadcastReceiver.addQueuedDownload(downloadReference);
        } catch (RuntimeException e) {
            Log.e(FRAGMENT_TAG, "Download failed: " + e);
        }
    }

    public boolean onBackPressed() {
        if (findInPageView.getVisibility() == View.VISIBLE) {
            hideFindInPage();
        } else if (canGoBack()) {
            // Go back in web history
            goBack();
        } else {
            if (session.getSource() == Source.VIEW || session.getSource() == Source.CUSTOM_TAB) {
                TelemetryWrapper.eraseBackToAppEvent();

                // This session has been started from a VIEW intent. Go back to the previous app
                // immediately and erase the current browsing session.
                erase();

                // If there are no other sessions then we remove the whole task because otherwise
                // the old session might still be partially visible in the app switcher.
                if (!SessionManager.getInstance().hasSession()) {
                    getActivity().finishAndRemoveTask();
                } else {
                    getActivity().finish();
                }

                // We can't show a snackbar outside of the app. So let's show a toast instead.
                Toast.makeText(getContext(), R.string.feedback_erase, Toast.LENGTH_SHORT).show();
            } else {
                // Just go back to the home screen.
                TelemetryWrapper.eraseBackToHomeEvent();

                erase();
            }
        }

        return true;
    }

    public void erase() {
        final IWebView webView = getWebView();
        if (webView != null) {
            webView.cleanup();
        }

        if (session.isCustomTab()) {
            SessionManager.getInstance().removeCustomTabSession(session.getUUID());
        } else {
            SessionManager.getInstance().removeCurrentSession();
        }
    }

    private void shareCurrentUrl() {
        final String url = getUrl();

        final Intent shareIntent = new Intent(Intent.ACTION_SEND);
        shareIntent.setType("text/plain");
        shareIntent.putExtra(Intent.EXTRA_TEXT, url);

        // Use title from webView if it's content matches the url
        final IWebView webView = getWebView();
        if (webView != null) {
            final String contentUrl = webView.getUrl();
            if (contentUrl != null && contentUrl.equals(url)) {
                final String contentTitle = webView.getTitle();
                shareIntent.putExtra(Intent.EXTRA_SUBJECT, contentTitle);
            }
        }

        startActivity(Intent.createChooser(shareIntent, getString(R.string.share_dialog_title)));

        TelemetryWrapper.shareEvent();
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.menuView:
                BrowserMenu menu = new BrowserMenu(getActivity(), this, session.getCustomTabConfig());
                menu.show(menuView);

                menuWeakReference = new WeakReference<>(menu);
                break;

            case R.id.display_url:
                if (SessionManager.getInstance().hasSessionWithUUID(session.getUUID())) {
                    final Fragment urlFragment = UrlInputFragment
                            .createWithSession(session, urlView);

                    getActivity().getSupportFragmentManager()
                            .beginTransaction()
                            .add(R.id.container, urlFragment, UrlInputFragment.FRAGMENT_TAG)
                            .commit();
                }
                break;

            case R.id.erase: {
                TelemetryWrapper.eraseEvent();

                erase();
                break;
            }

            case R.id.tabs:
                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .add(R.id.container, new SessionsSheetFragment(), SessionsSheetFragment.FRAGMENT_TAG)
                        .commit();

                TelemetryWrapper.openTabsTrayEvent();
                break;

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

                TelemetryWrapper.menuReloadEvent();
                break;
            }

            case R.id.stop: {
                final IWebView webView = getWebView();
                if (webView != null) {
                    webView.stopLoading();
                }
                break;
            }

            case R.id.open_in_firefox_focus: {
                sessionManager.moveCustomTabToRegularSessions(session);

                final IWebView webView = getWebView();
                if (webView != null) {
                    webView.saveWebViewState(session);
                }

                final Intent intent = new Intent(getContext(), MainActivity.class);
                intent.setAction(Intent.ACTION_MAIN);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);

                TelemetryWrapper.openFullBrowser();

                final Activity activity = getActivity();
                if (activity != null) {
                    activity.finish();
                }

                break;
            }

            case R.id.share: {
                shareCurrentUrl();
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

                if (browsers.isFirefoxDefaultBrowser()) {
                    TelemetryWrapper.openFirefoxEvent();
                } else {
                    TelemetryWrapper.openDefaultAppEvent();
                }
                break;
            }

            case R.id.open_select_browser: {
                final Browsers browsers = new Browsers(getContext(), getUrl());

                final ActivityInfo[] apps = browsers.getInstalledBrowsers();
                final ActivityInfo store = browsers.hasFirefoxBrandedBrowserInstalled()
                        ? null
                        : InstallFirefoxActivity.resolveAppStore(getContext());

                final OpenWithFragment fragment = OpenWithFragment.newInstance(
                        apps,
                        getUrl(),
                        store);
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
                SessionManager.getInstance().createSession(Source.MENU, SupportUtils.HELP_URL);
                break;

            case R.id.help_trackers:
                SessionManager.getInstance().createSession(Source.MENU,
                        SupportUtils.getSumoURLForTopic(getContext(), SupportUtils.SumoTopic.TRACKERS));
                break;

            case R.id.add_to_homescreen: {
                final IWebView webView = getWebView();
                if (webView == null) {
                    break;
                }

                final String url = webView.getUrl();
                final String title = webView.getTitle();
                showAddToHomescreenDialog(url, title);
                break;
            }

            case R.id.security_info:
                showSecurityPopUp();
                break;

            case R.id.report_site_issue:
                SessionManager.getInstance()
                        .createSession(Source.MENU,
                                String.format(SupportUtils.REPORT_SITE_ISSUE_URL, getUrl()));
                TelemetryWrapper.reportSiteIssueEvent();
                break;

            case R.id.find_in_page:
                showFindInPage();
                TelemetryWrapper.findInPageMenuEvent();
                break;

            case R.id.queryText:
                findInPageQuery.setCursorVisible(true);
                break;

            case R.id.nextResult: {
                ViewUtils.hideKeyboard(findInPageQuery);
                findInPageQuery.setCursorVisible(false);

                final IWebView webView = getWebView();
                if (webView == null) {
                    break;
                }

                webView.findNext(true);
                break;
            }

            case R.id.previousResult: {
                ViewUtils.hideKeyboard(findInPageQuery);
                findInPageQuery.setCursorVisible(false);

                final IWebView webView = getWebView();
                if (webView == null) {
                    break;
                }

                webView.findNext(false);
                break;
            }

            case R.id.close_find_in_page: {
                hideFindInPage();

                break;
            }

            default:
                throw new IllegalArgumentException("Unhandled menu item in BrowserFragment");
        }
    }

    private void updateToolbarButtonStates(boolean isLoading) {
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
        if (webView != null && !TextUtils.isEmpty(url)) {
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

        statusBar.setBackgroundResource(enabled ? R.drawable.animated_background : R.drawable.animated_background_disabled);

        if (!session.isCustomTab()) {
            // Only update the toolbar background if this is not a custom tab. Custom tabs set their
            // own color and we do not want to override this here.
            urlBar.setBackgroundResource(enabled ? R.drawable.animated_background : R.drawable.animated_background_disabled);

            backgroundTransitionGroup = new TransitionDrawableGroup(
                    (TransitionDrawable) urlBar.getBackground(),
                    (TransitionDrawable) statusBar.getBackground()
            );
        } else {
            backgroundTransitionGroup = new TransitionDrawableGroup(
                    (TransitionDrawable) statusBar.getBackground()
            );
        }
    }

    public void setShouldRequestDesktop(boolean enabled) {
        final IWebView webView = getWebView();
        if (webView != null) {
            webView.setRequestDesktop(enabled);
        }
    }

    private void showSecurityPopUp() {
        // Don't show Security Popup if the page is loading
        if (session.getLoading().getValue()) {
            return;
        }
        final PopupWindow securityPopup = PopupUtils.INSTANCE.createSecurityPopup(getContext(), session);
        if (securityPopup != null) {
            securityPopup.setOnDismissListener(new PopupWindow.OnDismissListener() {
                @Override
                public void onDismiss() {
                    popupTint.setVisibility(View.GONE);
                }
            });
            securityPopup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            securityPopup.setAnimationStyle(android.R.style.Animation_Dialog);
            securityPopup.setTouchable(true);
            securityPopup.setFocusable(true);
            securityPopup.setElevation(getResources().getDimension(R.dimen.menu_elevation));
            final int offsetY = getContext().getResources().getDimensionPixelOffset(R.dimen.doorhanger_offsetY);
            securityPopup.showAtLocation(urlBar, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, offsetY);
            popupTint.setVisibility(View.VISIBLE);
        }
    }

    // In the future, if more badging icons are needed, this should be abstracted
    public void updateBlockingBadging(boolean enabled) {
        blockView.setVisibility(enabled ? View.GONE : View.VISIBLE);
    }

    private void dismissAutocompletePopup() {
        autocompletePopupWeakReference.get().dismiss();
        autocompletePopupWeakReference.clear();
    }

    public boolean onLongClick(View view) {
        // Detect long clicks on display_url
        if (view.getId() == R.id.display_url) {
            Context context = getActivity();
            if (context == null) {
                return false;
            }

            if (session.isCustomTab()) {
                ClipboardManager clipBoard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
                if (clipBoard != null) {
                    final Uri uri = Uri.parse(getUrl());
                    clipBoard.setPrimaryClip(ClipData.newRawUri("Uri", uri));
                    Toast.makeText(context, getString(R.string.custom_tab_copy_url_action), Toast.LENGTH_SHORT).show();
                }
            }

            AutocompleteQuickAddPopup autocompletePopup = new AutocompleteQuickAddPopup(context, urlView.getText().toString());

            // Show the Snackbar and dismiss the popup when a new URL is added.
            autocompletePopup.setOnUrlAdded(new Function1<Boolean, Unit>() {
                @Override
                public Unit invoke(final Boolean didAddSuccessfully) {
                    getActivity().runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            final int messageId = didAddSuccessfully ? R.string.preference_autocomplete_add_confirmation : R.string.preference_autocomplete_duplicate_url_error;
                            ViewUtils.showBrandedSnackbar(Objects.requireNonNull(getView()), messageId, 0);
                            dismissAutocompletePopup();
                        }
                    });

                    return Unit.INSTANCE;
                }
            });

            autocompletePopup.show(urlView);
            autocompletePopupWeakReference = new WeakReference<>(autocompletePopup);
        }

        return false;
    }

    private void showFindInPage() {
        findInPageView.setVisibility(View.VISIBLE);
        findInPageQuery.requestFocus();

        CoordinatorLayout.LayoutParams params = (CoordinatorLayout.LayoutParams) swipeRefresh.getLayoutParams();
        params.bottomMargin = findInPageViewHeight;
        swipeRefresh.setLayoutParams(params);
    }

    private void hideFindInPage() {
        final IWebView webView = getWebView();
        if (webView == null) {
            return;
        }

        webView.clearMatches();
        findInPageCoordinator.reset();
        findInPageView.setVisibility(View.GONE);
        findInPageQuery.setText("");
        findInPageQuery.clearFocus();

        CoordinatorLayout.LayoutParams params = (CoordinatorLayout.LayoutParams) swipeRefresh.getLayoutParams();
        params.bottomMargin = 0;
        swipeRefresh.setLayoutParams(params);
        ViewUtils.hideKeyboard(findInPageQuery);
    }

    private void updateFindInPageResult(Integer activeMatchOrdinal, Integer numberOfMatches) {
        final Context context = getContext();
        if (context == null) {
            return;
        }

        if (numberOfMatches > 0) {
            // We don't want the presentation of the activeMatchOrdinal to be zero indexed. So let's
            // increment it by one.
            findInPageNext.setColorFilter(getResources().getColor(R.color.photonWhite));
            findInPageNext.setAlpha(1.0F);
            findInPagePrevious.setColorFilter(getResources().getColor(R.color.photonWhite));
            findInPagePrevious.setAlpha(1.0F);
            activeMatchOrdinal++;
            final String visibleString = String.format(context.getString(R.string.find_in_page_result), activeMatchOrdinal, numberOfMatches);
            final String accessibleString = String.format(context.getString(R.string.find_in_page_result), activeMatchOrdinal, numberOfMatches);

            findInPageResultTextView.setText(visibleString);
            findInPageResultTextView.setContentDescription(accessibleString);
        } else {
            findInPageNext.setColorFilter(getResources().getColor(R.color.photonGrey10));
            findInPageNext.setAlpha(0.4F);
            findInPagePrevious.setColorFilter(getResources().getColor(R.color.photonWhite));
            findInPagePrevious.setAlpha(0.4F);
            findInPageResultTextView.setText("");
            findInPageResultTextView.setContentDescription("");
        }
    }
}
