/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.ui;

import java.net.URISyntaxException;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.overlays.OverlayConstants;
import org.mozilla.gecko.overlays.service.OverlayActionService;
import org.mozilla.gecko.overlays.service.sharemethods.SendTab;
import org.mozilla.gecko.overlays.service.sharemethods.ShareMethod;
import org.mozilla.gecko.sync.setup.activities.WebURLFinder;
import org.mozilla.gecko.util.IntentUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Parcelable;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;
import android.view.animation.AnimationSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.TextView;
import android.widget.Toast;

/**
 * A transparent activity that displays the share overlay.
 */
public class ShareDialog extends Locales.LocaleAwareActivity implements SendTabTargetSelectedListener {

    private enum State {
        DEFAULT,
        DEVICES_ONLY // Only display the device list.
    }

    private static final String LOGTAG = "GeckoShareDialog";

    /** Flag to indicate that we should always show the device list; specific to this release channel. **/
    public static final String INTENT_EXTRA_DEVICES_ONLY =
            AppConstants.ANDROID_PACKAGE_NAME + ".intent.extra.DEVICES_ONLY";

    /** The maximum number of devices we'll show in the dialog when in State.DEFAULT. **/
    private static final int MAXIMUM_INLINE_DEVICES = 2;

    private State state;

    private SendTabList sendTabList;
    private OverlayDialogButton bookmarkButton;

    // The bookmark button drawable set from XML - we need this to reset state.
    private Drawable bookmarkButtonDrawable;

    private String url;
    private String title;

    // The override intent specified by SendTab (if any). See SendTab.java.
    private Intent sendTabOverrideIntent;

    // Flag set during animation to prevent animation multiple-start.
    private boolean isAnimating;

    // BroadcastReceiver to receive callbacks from ShareMethods which are changing state.
    private final BroadcastReceiver uiEventListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            ShareMethod.Type originShareMethod = intent.getParcelableExtra(OverlayConstants.EXTRA_SHARE_METHOD);
            switch (originShareMethod) {
                case SEND_TAB:
                    handleSendTabUIEvent(intent);
                    break;
                default:
                    throw new IllegalArgumentException("UIEvent broadcast from ShareMethod that isn't thought to support such broadcasts.");
            }
        }
    };

    /**
     * Called when a UI event broadcast is received from the SendTab ShareMethod.
     */
    protected void handleSendTabUIEvent(Intent intent) {
        sendTabOverrideIntent = intent.getParcelableExtra(SendTab.OVERRIDE_INTENT);

        RemoteClient[] remoteClientRecords = (RemoteClient[]) intent.getParcelableArrayExtra(SendTab.EXTRA_REMOTE_CLIENT_RECORDS);

        // Escape hatch: we don't show the option to open this dialog in this state so this should
        // never be run. However, due to potential inconsistencies in synced client state
        // (e.g. bug 1122302 comment 47), we might fail.
        if (state == State.DEVICES_ONLY &&
                (remoteClientRecords == null || remoteClientRecords.length == 0)) {
            Log.e(LOGTAG, "In state: " + State.DEVICES_ONLY + " and received 0 synced clients. Finishing...");
            Toast.makeText(this, getResources().getText(R.string.overlay_no_synced_devices), Toast.LENGTH_SHORT)
                 .show();
            finish();
            return;
        }

        sendTabList.setSyncClients(remoteClientRecords);

        if (state == State.DEVICES_ONLY ||
                remoteClientRecords == null ||
                remoteClientRecords.length <= MAXIMUM_INLINE_DEVICES) {
            // Show the list of devices in-line.
            sendTabList.switchState(SendTabList.State.LIST);

            // The first item in the list has a unique style. If there are no items
            // in the list, the next button appears to be the first item in the list.
            //
            // Note: a more thorough implementation would add this
            // (and other non-ListView buttons) into a custom ListView.
            if (remoteClientRecords == null || remoteClientRecords.length == 0) {
                bookmarkButton.setBackgroundResource(
                        R.drawable.overlay_share_button_background_first);
            }
            return;
        }

        // Just show a button to launch the list of devices to choose from.
        sendTabList.switchState(SendTabList.State.SHOW_DEVICES);
    }

    @Override
    protected void onDestroy() {
        // Remove the listener when the activity is destroyed: we no longer care.
        // Note: The activity can be destroyed without onDestroy being called. However, this occurs
        // only when the application is killed, something which also kills the registered receiver
        // list, and the service, and everything else: so we don't care.
        LocalBroadcastManager.getInstance(this).unregisterReceiver(uiEventListener);

        super.onDestroy();
    }

    /**
     * Show a toast indicating we were started with no URL, and then stop.
     */
    private void abortDueToNoURL() {
        Log.e(LOGTAG, "Unable to process shared intent. No URL found!");

        // Display toast notifying the user of failure (most likely a developer who screwed up
        // trying to send a share intent).
        Toast toast = Toast.makeText(this, getResources().getText(R.string.overlay_share_no_url), Toast.LENGTH_SHORT);
        toast.show();
        finish();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.overlay_share_dialog);

        LocalBroadcastManager.getInstance(this).registerReceiver(uiEventListener,
                new IntentFilter(OverlayConstants.SHARE_METHOD_UI_EVENT));

        // Send tab.
        sendTabList = (SendTabList) findViewById(R.id.overlay_send_tab_btn);

        // Register ourselves as both the listener and the context for the Adapter.
        final SendTabDeviceListArrayAdapter adapter = new SendTabDeviceListArrayAdapter(this, this);
        sendTabList.setAdapter(adapter);
        sendTabList.setSendTabTargetSelectedListener(this);

        bookmarkButton = (OverlayDialogButton) findViewById(R.id.overlay_share_bookmark_btn);

        bookmarkButtonDrawable = bookmarkButton.getBackground();

        // Bookmark button
        bookmarkButton = (OverlayDialogButton) findViewById(R.id.overlay_share_bookmark_btn);
        bookmarkButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                addBookmark();
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();

        final Intent intent = getIntent();

        state = intent.getBooleanExtra(INTENT_EXTRA_DEVICES_ONLY, false) ?
                State.DEVICES_ONLY : State.DEFAULT;

        // If the Activity is being reused, we need to reset the state. Ideally, we create a
        // new instance for each call, but Android L breaks this (bug 1137928).
        sendTabList.switchState(SendTabList.State.LOADING);
        bookmarkButton.setBackgroundDrawable(bookmarkButtonDrawable);

        // The URL is usually hiding somewhere in the extra text. Extract it.
        final String extraText = IntentUtils.getStringExtraSafe(intent, Intent.EXTRA_TEXT);
        if (TextUtils.isEmpty(extraText)) {
            abortDueToNoURL();
            return;
        }

        final String pageUrl = new WebURLFinder(extraText).bestWebURL();
        if (TextUtils.isEmpty(pageUrl)) {
            abortDueToNoURL();
            return;
        }

        // Have the service start any initialisation work that's necessary for us to show the correct
        // UI. The results of such work will come in via the BroadcastListener.
        Intent serviceStartupIntent = new Intent(this, OverlayActionService.class);
        serviceStartupIntent.setAction(OverlayConstants.ACTION_PREPARE_SHARE);
        startService(serviceStartupIntent);

        // Start the slide-up animation.
        getWindow().setWindowAnimations(0);
        final Animation anim = AnimationUtils.loadAnimation(this, R.anim.overlay_slide_up);
        findViewById(R.id.sharedialog).startAnimation(anim);

        // If provided, we use the subject text to give us something nice to display.
        // If not, we wing it with the URL.

        // TODO: Consider polling Fennec databases to find better information to display.
        final String subjectText = intent.getStringExtra(Intent.EXTRA_SUBJECT);

        final String telemetryExtras = "title=" + (subjectText != null);
        if (subjectText != null) {
            ((TextView) findViewById(R.id.title)).setText(subjectText);
        }

        Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.SHARE_OVERLAY, telemetryExtras);

        title = subjectText;
        url = pageUrl;

        // Set the subtitle text on the view and cause it to marquee if it's too long (which it will
        // be, since it's a URL).
        final TextView subtitleView = (TextView) findViewById(R.id.subtitle);
        subtitleView.setText(pageUrl);
        subtitleView.setEllipsize(TextUtils.TruncateAt.MARQUEE);
        subtitleView.setSingleLine(true);
        subtitleView.setMarqueeRepeatLimit(5);
        subtitleView.setSelected(true);

        final View titleView = findViewById(R.id.title);

        if (state == State.DEVICES_ONLY) {
            bookmarkButton.setVisibility(View.GONE);

            titleView.setOnClickListener(null);
            subtitleView.setOnClickListener(null);
            return;
        }

        bookmarkButton.setVisibility(View.VISIBLE);

        // Configure buttons.
        final View.OnClickListener launchBrowser = new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                ShareDialog.this.launchBrowser();
            }
        };

        titleView.setOnClickListener(launchBrowser);
        subtitleView.setOnClickListener(launchBrowser);

        final LocalBrowserDB browserDB = new LocalBrowserDB(getCurrentProfile());
        setButtonState(url, browserDB);
    }

    @Override
    protected void onNewIntent(final Intent intent) {
        super.onNewIntent(intent);

        // The intent returned by getIntent is not updated automatically.
        setIntent(intent);
    }

    /**
     * Sets the state of the bookmark/reading list buttons: they are disabled if the given URL is
     * already in the corresponding list.
     */
    private void setButtonState(final String pageURL, final LocalBrowserDB browserDB) {
        new UIAsyncTask.WithoutParams<Void>(ThreadUtils.getBackgroundHandler()) {
            // Flags to hold the result
            boolean isBookmark;

            @Override
            protected Void doInBackground() {
                final ContentResolver contentResolver = getApplicationContext().getContentResolver();

                isBookmark = browserDB.isBookmark(contentResolver, pageURL);

                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                findViewById(R.id.overlay_share_bookmark_btn).setEnabled(!isBookmark);
            }
        }.execute();
    }

    /**
     * Helper method to get an overlay service intent populated with the data held in this dialog.
     */
    private Intent getServiceIntent(ShareMethod.Type method) {
        final Intent serviceIntent = new Intent(this, OverlayActionService.class);
        serviceIntent.setAction(OverlayConstants.ACTION_SHARE);

        serviceIntent.putExtra(OverlayConstants.EXTRA_SHARE_METHOD, (Parcelable) method);
        serviceIntent.putExtra(OverlayConstants.EXTRA_URL, url);
        serviceIntent.putExtra(OverlayConstants.EXTRA_TITLE, title);

        return serviceIntent;
    }

    @Override
    public void finish() {
        finish(true);
    }

    private void finish(final boolean shouldOverrideAnimations) {
        super.finish();
        if (shouldOverrideAnimations) {
            // Don't perform an activity-dismiss animation.
            overridePendingTransition(0, 0);
        }
    }

    /*
     * Button handlers. Send intents to the background service responsible for processing requests
     * on Fennec in the background. (a nice extensible mechanism for "doing stuff without properly
     * launching Fennec").
     */

    @Override
    public void onSendTabActionSelected() {
        // This requires an override intent.
        if (sendTabOverrideIntent == null) {
            throw new IllegalStateException("sendTabOverrideIntent must not be null");
        }

        startActivity(sendTabOverrideIntent);
        finish();
    }

    @Override
    public void onSendTabTargetSelected(String targetGUID) {
        // targetGUID being null with no override intent should be an impossible state.
        if (targetGUID == null) {
            throw new IllegalStateException("targetGUID must not be null");
        }

        Intent serviceIntent = getServiceIntent(ShareMethod.Type.SEND_TAB);

        // Currently, only one extra parameter is necessary (the GUID of the target device).
        Bundle extraParameters = new Bundle();

        // Future: Handle multiple-selection. Bug 1061297.
        extraParameters.putStringArray(SendTab.SEND_TAB_TARGET_DEVICES, new String[] { targetGUID });

        serviceIntent.putExtra(OverlayConstants.EXTRA_PARAMETERS, extraParameters);

        startService(serviceIntent);
        animateOut(true);

        Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.SHARE_OVERLAY, "sendtab");
    }

    public void addBookmark() {
        startService(getServiceIntent(ShareMethod.Type.ADD_BOOKMARK));
        animateOut(true);

        Telemetry.sendUIEvent(TelemetryContract.Event.SAVE, TelemetryContract.Method.SHARE_OVERLAY, "bookmark");
    }

    public void launchBrowser() {
        try {
            // This can launch in the guest profile. Sorry.
            final Intent i = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
            i.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
            startActivity(i);
        } catch (URISyntaxException e) {
            // Nothing much we can do.
        } finally {
            // Since we're changing apps, users expect the default app switch animations.
            finish(false);
        }
    }

    private String getCurrentProfile() {
        return GeckoProfile.DEFAULT_PROFILE;
    }

    /**
     * Slide the overlay down off the screen, display
     * a check (if given), and finish the activity.
     */
    private void animateOut(final boolean shouldDisplayConfirmation) {
        if (isAnimating) {
            return;
        }

        isAnimating = true;
        final Animation slideOutAnim = AnimationUtils.loadAnimation(this, R.anim.overlay_slide_down);

        final Animation animationToFinishActivity;
        if (!shouldDisplayConfirmation) {
            animationToFinishActivity = slideOutAnim;
        } else {
            final View check = findViewById(R.id.check);
            check.setVisibility(View.VISIBLE);
            final Animation checkEntryAnim = AnimationUtils.loadAnimation(this, R.anim.overlay_check_entry);
            final Animation checkExitAnim = AnimationUtils.loadAnimation(this, R.anim.overlay_check_exit);
            checkExitAnim.setStartOffset(checkEntryAnim.getDuration() + 500);

            final AnimationSet checkAnimationSet = new AnimationSet(this, null);
            checkAnimationSet.addAnimation(checkEntryAnim);
            checkAnimationSet.addAnimation(checkExitAnim);

            check.startAnimation(checkAnimationSet);
            animationToFinishActivity = checkExitAnim;
        }

        findViewById(R.id.sharedialog).startAnimation(slideOutAnim);
        animationToFinishActivity.setAnimationListener(new Animation.AnimationListener() {
            @Override
            public void onAnimationStart(Animation animation) { /* Unused. */ }

            @Override
            public void onAnimationEnd(Animation animation) {
                finish();
            }

            @Override
            public void onAnimationRepeat(Animation animation) { /* Unused. */ }
        });

        // Allows the user to dismiss the animation early.
        setFullscreenFinishOnClickListener();
    }

    /**
     * Sets a fullscreen {@link #finish()} click listener. We do this rather than attaching an
     * onClickListener to the root View because in that case, we need to remove all of the
     * existing listeners, which is less robust.
     */
    private void setFullscreenFinishOnClickListener() {
        final View clickTarget = findViewById(R.id.fullscreen_click_target);
        clickTarget.setVisibility(View.VISIBLE);
        clickTarget.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                finish();
            }
        });
    }

    /**
     * Close the dialog if back is pressed.
     */
    @Override
    public void onBackPressed() {
        animateOut(false);
        Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, TelemetryContract.Method.SHARE_OVERLAY);
    }

    /**
     * Close the dialog if the anything that isn't a button is tapped.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        animateOut(false);
        Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, TelemetryContract.Method.SHARE_OVERLAY);
        return true;
    }
}
