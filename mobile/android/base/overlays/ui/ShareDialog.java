/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.ui;

import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Parcelable;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.TextView;
import android.widget.Toast;

import org.mozilla.gecko.Assert;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.overlays.OverlayConstants;
import org.mozilla.gecko.overlays.service.OverlayActionService;
import org.mozilla.gecko.overlays.service.sharemethods.ParcelableClientRecord;
import org.mozilla.gecko.overlays.service.sharemethods.SendTab;
import org.mozilla.gecko.overlays.service.sharemethods.ShareMethod;
import org.mozilla.gecko.LocaleAware;
import org.mozilla.gecko.sync.setup.activities.WebURLFinder;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

/**
 * A transparent activity that displays the share overlay.
 */
public class ShareDialog extends LocaleAware.LocaleAwareActivity implements SendTabTargetSelectedListener {
    private static final String LOGTAG = "GeckoShareDialog";

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

        SendTabList sendTabList = (SendTabList) findViewById(R.id.overlay_send_tab_btn);

        ParcelableClientRecord[] clientrecords = (ParcelableClientRecord[]) intent.getParcelableArrayExtra(SendTab.EXTRA_CLIENT_RECORDS);
        sendTabList.setSyncClients(clientrecords);
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

        getWindow().setWindowAnimations(0);

        Intent intent = getIntent();
        final Resources resources = getResources();

        // The URL is usually hiding somewhere in the extra text. Extract it.
        final String extraText = intent.getStringExtra(Intent.EXTRA_TEXT);
        if (TextUtils.isEmpty(extraText)) {
            abortDueToNoURL();
            return;
        }

        final String pageUrl = new WebURLFinder(extraText).bestWebURL();
        if (TextUtils.isEmpty(pageUrl)) {
            abortDueToNoURL();
            return;
        }

        setContentView(R.layout.overlay_share_dialog);


        LocalBroadcastManager.getInstance(this).registerReceiver(uiEventListener,
                                                                  new IntentFilter(OverlayConstants.SHARE_METHOD_UI_EVENT));

        // Have the service start any initialisation work that's necessary for us to show the correct
        // UI. The results of such work will come in via the BroadcastListener.
        Intent serviceStartupIntent = new Intent(this, OverlayActionService.class);
        serviceStartupIntent.setAction(OverlayConstants.ACTION_PREPARE_SHARE);
        startService(serviceStartupIntent);

        // If provided, we use the subject text to give us something nice to display.
        // If not, we wing it with the URL.
        // TODO: Consider polling Fennec databases to find better information to display.
        String subjectText = intent.getStringExtra(Intent.EXTRA_SUBJECT);
        if (subjectText != null) {
            ((TextView) findViewById(R.id.title)).setText(subjectText);
        }

        title = subjectText;
        url = pageUrl;

        // Set the subtitle text on the view and cause it to marquee if it's too long (which it will
        // be, since it's a URL).
        TextView subtitleView = (TextView) findViewById(R.id.subtitle);
        subtitleView.setText(pageUrl);
        subtitleView.setEllipsize(TextUtils.TruncateAt.MARQUEE);
        subtitleView.setSingleLine(true);
        subtitleView.setMarqueeRepeatLimit(5);
        subtitleView.setSelected(true);

        // Start the slide-up animation.
        Animation anim = AnimationUtils.loadAnimation(this, R.anim.overlay_slide_up);
        findViewById(R.id.sharedialog).startAnimation(anim);

        // Configure buttons.
        final OverlayDialogButton bookmarkBtn = (OverlayDialogButton) findViewById(R.id.overlay_share_bookmark_btn);

        final String bookmarkEnabledLabel = resources.getString(R.string.overlay_share_bookmark_btn_label);
        final Drawable bookmarkEnabledIcon = resources.getDrawable(R.drawable.overlay_bookmark_icon);
        bookmarkBtn.setEnabledLabelAndIcon(bookmarkEnabledLabel, bookmarkEnabledIcon);

        final String bookmarkDisabledLabel = resources.getString(R.string.overlay_share_bookmark_btn_label_already);
        final Drawable bookmarkDisabledIcon = resources.getDrawable(R.drawable.overlay_bookmarked_already_icon);
        bookmarkBtn.setDisabledLabelAndIcon(bookmarkDisabledLabel, bookmarkDisabledIcon);

        bookmarkBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                addBookmark();
            }
        });

        // Send tab.
        SendTabList sendTabList = (SendTabList) findViewById(R.id.overlay_send_tab_btn);

        // Register ourselves as both the listener and the context for the Adapter.
        SendTabDeviceListArrayAdapter adapter = new SendTabDeviceListArrayAdapter(this, this);
        sendTabList.setAdapter(adapter);
        sendTabList.setSendTabTargetSelectedListener(this);

        // If we're a low memory device, just hide the reading list button. Otherwise, configure it.
        final OverlayDialogButton readinglistBtn = (OverlayDialogButton) findViewById(R.id.overlay_share_reading_list_btn);

        if (HardwareUtils.isLowMemoryPlatform()) {
            readinglistBtn.setVisibility(View.GONE);
            return;
        }

        final String readingListEnabledLabel = resources.getString(R.string.overlay_share_reading_list_btn_label);
        final Drawable readingListEnabledIcon = resources.getDrawable(R.drawable.overlay_readinglist_icon);
        readinglistBtn.setEnabledLabelAndIcon(readingListEnabledLabel, readingListEnabledIcon);

        final String readingListDisabledLabel = resources.getString(R.string.overlay_share_reading_list_btn_label_already);
        final Drawable readingListDisabledIcon = resources.getDrawable(R.drawable.overlay_readinglist_already_icon);
        readinglistBtn.setDisabledLabelAndIcon(readingListDisabledLabel, readingListDisabledIcon);

        readinglistBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                addToReadingList();
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();

        LocalBrowserDB browserDB = new LocalBrowserDB(getCurrentProfile());
        disableButtonsIfAlreadyAdded(url, browserDB);
    }

    /**
     * Disables the bookmark/reading list buttons if the given URL is already in the corresponding
     * list.
     */
    private void disableButtonsIfAlreadyAdded(final String pageURL, final LocalBrowserDB browserDB) {
        new UIAsyncTask.WithoutParams<Void>(ThreadUtils.getBackgroundHandler()) {
            // Flags to hold the result
            boolean isBookmark;
            boolean isReadingListItem;

            @Override
            protected Void doInBackground() {
                final ContentResolver contentResolver = getApplicationContext().getContentResolver();

                isBookmark = browserDB.isBookmark(contentResolver, pageURL);
                isReadingListItem = browserDB.isReadingListItem(contentResolver, pageURL);

                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                findViewById(R.id.overlay_share_bookmark_btn).setEnabled(!isBookmark);
                findViewById(R.id.overlay_share_reading_list_btn).setEnabled(!isReadingListItem);
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
        super.finish();

        // Don't perform an activity-dismiss animation.
        overridePendingTransition(0, 0);
    }

    /*
     * Button handlers. Send intents to the background service responsible for processing requests
     * on Fennec in the background. (a nice extensible mechanism for "doing stuff without properly
     * launching Fennec").
     */

    public void sendTab(String targetGUID) {
        // If an override intent has been set, dispatch it.
        if (sendTabOverrideIntent != null) {
            startActivity(sendTabOverrideIntent);
            finish();
            return;
        }

        // targetGUID being null with no override intent should be an impossible state.
        Assert.isTrue(targetGUID != null);

        Intent serviceIntent = getServiceIntent(ShareMethod.Type.SEND_TAB);

        // Currently, only one extra parameter is necessary (the GUID of the target device).
        Bundle extraParameters = new Bundle();

        // Future: Handle multiple-selection. Bug 1061297.
        extraParameters.putStringArray(SendTab.SEND_TAB_TARGET_DEVICES, new String[] { targetGUID });

        serviceIntent.putExtra(OverlayConstants.EXTRA_PARAMETERS, extraParameters);

        startService(serviceIntent);
        slideOut();
    }

    @Override
    public void onSendTabTargetSelected(String targetGUID) {
        sendTab(targetGUID);
    }

    public void addToReadingList() {
        startService(getServiceIntent(ShareMethod.Type.ADD_TO_READING_LIST));
        slideOut();
    }

    public void addBookmark() {
        startService(getServiceIntent(ShareMethod.Type.ADD_BOOKMARK));
        slideOut();
    }

    private String getCurrentProfile() {
        return GeckoProfile.DEFAULT_PROFILE;
    }

    /**
     * Slide the overlay down off the screen and destroy it.
     */
    private void slideOut() {
        if (isAnimating) {
            return;
        }

        isAnimating = true;
        Animation anim = AnimationUtils.loadAnimation(this, R.anim.overlay_slide_down);
        findViewById(R.id.sharedialog).startAnimation(anim);

        anim.setAnimationListener(new Animation.AnimationListener() {
            @Override
            public void onAnimationStart(Animation animation) {
                // Unused. I can haz Miranda method?
            }

            @Override
            public void onAnimationEnd(Animation animation) {
                finish();
            }

            @Override
            public void onAnimationRepeat(Animation animation) {
                // Unused.
            }
        });
    }

    /**
     * Close the dialog if back is pressed.
     */
    @Override
    public void onBackPressed() {
        slideOut();
    }

    /**
     * Close the dialog if the anything that isn't a button is tapped.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        slideOut();
        return true;
    }
}
