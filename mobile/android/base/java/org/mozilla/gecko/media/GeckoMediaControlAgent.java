/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.media;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.media.AudioManager;
import android.media.VolumeProvider;
import android.media.session.MediaController;
import android.media.session.MediaSession;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.CheckResult;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.support.v4.app.NotificationManagerCompat;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.IntentHelper;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import static org.mozilla.gecko.BuildConfig.DEBUG;

public class GeckoMediaControlAgent {
    private static final String LOGTAG = "GeckoMediaControlAgent";

    @SuppressLint("StaticFieldLeak")
    private static GeckoMediaControlAgent instance;
    private Context mContext;

    public static final String ACTION_RESUME         = "action_resume";
    public static final String ACTION_PAUSE          = "action_pause";
    public static final String ACTION_STOP           = "action_stop";
    /* package */ static final String ACTION_RESUME_BY_AUDIO_FOCUS = "action_resume_audio_focus";
    /* package */ static final String ACTION_PAUSE_BY_AUDIO_FOCUS  = "action_pause_audio_focus";
    /* package */ static final String ACTION_START_AUDIO_DUCK      = "action_start_audio_duck";
    /* package */ static final String ACTION_STOP_AUDIO_DUCK       = "action_stop_audio_duck";
    /* package */ static final String ACTION_TAB_STATE_PLAYING = "action_tab_state_playing";
    /* package */ static final String ACTION_TAB_STATE_STOPPED = "action_tab_state_stopped";
    /* package */ static final String ACTION_TAB_STATE_RESUMED = "action_tab_state_resumed";
    /* package */ static final String ACTION_TAB_STATE_FAVICON = "action_tab_state_favicon";
    private static final String MEDIA_CONTROL_PREF = "dom.audiochannel.mediaControl";

    // This is maximum volume level difference when audio ducking. The number is arbitrary.
    private static final int AUDIO_DUCK_MAX_STEPS = 3;
    private enum AudioDucking { START, STOP }
    private boolean mSupportsDucking = false;
    private int mAudioDuckCurrentSteps = 0;

    private MediaSession mSession;
    private MediaController mController;
    private HeadSetStateReceiver mHeadSetStateReceiver;

    private PrefsHelper.PrefHandler mPrefsObserver;
    private final String[] mPrefs = { MEDIA_CONTROL_PREF };

    private boolean mInitialized = false;
    private boolean mIsMediaControlPrefOn = true;

    private int minCoverSize;
    private int coverSize;

    private Notification currentNotification;

    /**
     * Internal state of MediaControlService, to indicate it is playing media, or paused...etc.
     */
    private static State sMediaState = State.STOPPED;

    protected enum State {
        PLAYING,
        PAUSED,
        STOPPED
    }

    @RobocopTarget
    public static GeckoMediaControlAgent getInstance() {
        if (instance == null) {
            instance = new GeckoMediaControlAgent();
        }

        return instance;
    }

    private GeckoMediaControlAgent() {}

    public void attachToContext(Context context) {
        if (isAttachedToContext()) {
            return;
        }

        mContext = context;
        initialize();
    }

    private boolean isAttachedToContext() {
        return (mContext != null);
    }

    private void initialize() {
        if (mInitialized) {
            return;
        }

        if (!isAndroidVersionLollipopOrHigher()) {
            return;
        }

        Log.d(LOGTAG, "initialize");
        getGeckoPreference();
        if (!initMediaSession()) {
            if (DEBUG) {
                Log.e(LOGTAG, "initialization fail!");
            }
            return;
        }

        coverSize = (int) mContext.getResources().getDimension(R.dimen.notification_media_cover);
        minCoverSize = mContext.getResources().getDimensionPixelSize(R.dimen.favicon_bg);

        mHeadSetStateReceiver = new HeadSetStateReceiver().registerReceiver(mContext);

        mInitialized = true;
    }

    private boolean isAndroidVersionLollipopOrHigher() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
    }

    private void getGeckoPreference() {
        mPrefsObserver = new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean value) {
                if (pref.equals(MEDIA_CONTROL_PREF)) {
                    mIsMediaControlPrefOn = value;

                    // If media is playing, we just need to create or remove
                    // the media control interface.
                    if (sMediaState.equals(State.PLAYING)) {
                        setState(mIsMediaControlPrefOn ? State.PLAYING : State.STOPPED);
                    }

                    // If turn off pref during pausing, except removing media
                    // interface, we also need to stop the service and notify
                    // gecko about that.
                    if (sMediaState.equals(State.PAUSED) &&
                            !mIsMediaControlPrefOn) {
                        handleAction(ACTION_STOP);
                    }
                }
            }
        };
        PrefsHelper.addObserver(mPrefs, mPrefsObserver);
    }

    private boolean initMediaSession() {
        // Android MediaSession is introduced since version L.
        try {
            mSession = new MediaSession(mContext,
                    "fennec media session");
            mController = new MediaController(mContext,
                    mSession.getSessionToken());
        } catch (IllegalStateException e) {
            if (DEBUG) {
                Log.e(LOGTAG, "can't create MediaSession and MediaController!");
            }
            return false;
        }

        int volumeControl = mController.getPlaybackInfo().getVolumeControl();
        if (volumeControl == VolumeProvider.VOLUME_CONTROL_ABSOLUTE ||
                volumeControl == VolumeProvider.VOLUME_CONTROL_RELATIVE) {
            mSupportsDucking = true;
        } else {
            if (DEBUG) {
                Log.w(LOGTAG, "initMediaSession, Session does not support volume absolute or relative volume control");
            }
        }

        mSession.setCallback(new MediaSession.Callback() {
            @Override
            public void onCustomAction(@NonNull String action, Bundle extras) {
                if (action.equals(ACTION_PAUSE_BY_AUDIO_FOCUS)) {
                    Log.d(LOGTAG, "Controller, pause by audio focus changed");
                    setState(State.PAUSED);
                } else if (action.equals(ACTION_RESUME_BY_AUDIO_FOCUS)) {
                    Log.d(LOGTAG, "Controller, resume by audio focus changed");
                    setState(State.PLAYING);
                }
            }

            @Override
            public void onPlay() {
                Log.d(LOGTAG, "Controller, onPlay");
                super.onPlay();
                setState(State.PLAYING);
                notifyObservers("mediaControl", "resumeMedia");
            }

            @Override
            public void onPause() {
                Log.d(LOGTAG, "Controller, onPause");
                super.onPause();
                setState(State.PAUSED);
                notifyObservers("mediaControl", "mediaControlPaused");
            }

            @Override
            public void onStop() {
                Log.d(LOGTAG, "Controller, onStop");
                super.onStop();
                setState(State.STOPPED);
                notifyObservers("mediaControl", "mediaControlStopped");
                AudioFocusAgent.getInstance().clearActiveMediaTab();
            }
        });
        mSession.setActive(true);
        return true;
    }

    private void notifyObservers(String topic, String data) {
        final GeckoBundle newStatusBundle = new GeckoBundle(1);
        newStatusBundle.putString(topic, data);
        EventDispatcher.getInstance().dispatch("MediaControlService:MediaPlayingStatus", newStatusBundle);

        GeckoAppShell.notifyObservers(topic, data);
    }

    private boolean isNeedToRemoveControlInterface(State state) {
        return state.equals(State.STOPPED);
    }

    private void setState(State newState) {
        sMediaState = newState;
        setMediaStateForTab(sMediaState.equals(State.PLAYING));
        onStateChanged();
    }

    private void setMediaStateForTab(boolean isTabPlaying) {
        final Tab tab = AudioFocusAgent.getInstance().getActiveMediaTab();
        if (tab == null) {
            return;
        }
        tab.setIsMediaPlaying(isTabPlaying);
    }

    private void onStateChanged() {
        if (!mInitialized) {
            return;
        }

        Log.d(LOGTAG, "onStateChanged, state = " + sMediaState);

        if (isNeedToRemoveControlInterface(sMediaState)) {
            stopForegroundService();
            NotificationManagerCompat.from(mContext).cancel(R.id.mediaControlNotification);
            release();
            return;
        }

        if (!mIsMediaControlPrefOn) {
            return;
        }

        final Tab tab = AudioFocusAgent.getInstance().getActiveMediaTab();

        if (tab == null || tab.isPrivate()) {
            return;
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                updateNotification(tab);
            }
        });
    }

    /* package */ static boolean isMediaPlaying() {
        return sMediaState.equals(State.PLAYING);
    }

    public void handleAction(String action) {
        if (action == null) {
            return;
        }

        if (!mInitialized && action.equals(ACTION_TAB_STATE_PLAYING)) {
            initialize();
        }

        if (!mInitialized) {
            return;
        }

        Log.d(LOGTAG, "HandleAction, action = " + action + ", mediaState = " + sMediaState);
        switch (action) {
            case ACTION_RESUME :
                mController.getTransportControls().play();
                break;
            case ACTION_PAUSE :
                mController.getTransportControls().pause();
                break;
            case ACTION_STOP :
                mController.getTransportControls().stop();
                break;
            case ACTION_PAUSE_BY_AUDIO_FOCUS :
                mController.getTransportControls().sendCustomAction(ACTION_PAUSE_BY_AUDIO_FOCUS, null);
                break;
            case ACTION_RESUME_BY_AUDIO_FOCUS :
                mController.getTransportControls().sendCustomAction(ACTION_RESUME_BY_AUDIO_FOCUS, null);
                break;
            case ACTION_START_AUDIO_DUCK :
                handleAudioDucking(AudioDucking.START);
                break;
            case ACTION_STOP_AUDIO_DUCK :
                handleAudioDucking(AudioDucking.STOP);
                break;
            case ACTION_TAB_STATE_PLAYING :
                setState(State.PLAYING);
                break;
            case ACTION_TAB_STATE_STOPPED :
                setState(State.STOPPED);
                break;
            case ACTION_TAB_STATE_RESUMED :
                if (!isMediaPlaying()) {
                    setState(State.PLAYING);
                }
                break;
            case ACTION_TAB_STATE_FAVICON :
                setState(isMediaPlaying() ? State.PLAYING : State.PAUSED);
                break;
        }
    }

    private void handleAudioDucking(AudioDucking audioDucking) {
        if (!mInitialized || !mSupportsDucking) {
            return;
        }

        int currentVolume = mController.getPlaybackInfo().getCurrentVolume();
        int maxVolume = mController.getPlaybackInfo().getMaxVolume();

        int adjustDirection;
        if (audioDucking == AudioDucking.START) {
            mAudioDuckCurrentSteps = Math.min(AUDIO_DUCK_MAX_STEPS, currentVolume);
            adjustDirection = AudioManager.ADJUST_LOWER;
        } else {
            mAudioDuckCurrentSteps = Math.min(mAudioDuckCurrentSteps, maxVolume - currentVolume);
            adjustDirection = AudioManager.ADJUST_RAISE;
        }

        for (int i = 0; i < mAudioDuckCurrentSteps; i++) {
            mController.adjustVolume(adjustDirection, 0);
        }
    }

    @SuppressLint("NewApi")
    private void setCurrentNotification(Tab tab, boolean onGoing, int visibility) {
        final Notification.MediaStyle style = new Notification.MediaStyle();
        style.setShowActionsInCompactView(0);

        final Notification.Builder notificationBuilder = new Notification.Builder(mContext)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setLargeIcon(generateCoverArt(tab))
                .setContentTitle(tab.getTitle())
                .setContentText(tab.getURL())
                .setContentIntent(createContentIntent(tab))
                .setDeleteIntent(createDeleteIntent())
                .setStyle(style)
                .addAction(createNotificationAction())
                .setOngoing(onGoing)
                .setShowWhen(false)
                .setWhen(0)
                .setVisibility(visibility);

        if (!AppConstants.Versions.preO) {
            notificationBuilder.setChannelId(GeckoApplication.getDefaultNotificationChannel().getId());
        }

        currentNotification = notificationBuilder.build();
    }

    /* package */ Notification getCurrentNotification() {
        return currentNotification;
    }

    private void updateNotification(Tab tab) {
        ThreadUtils.assertNotOnUiThread();

        final boolean isPlaying = isMediaPlaying();
        final int visibility = tab.isPrivate() ? Notification.VISIBILITY_PRIVATE : Notification.VISIBILITY_PUBLIC;
        setCurrentNotification(tab, isPlaying, visibility);

        if (isPlaying) {
            startForegroundService();
        } else {
            stopForegroundService();
            NotificationManagerCompat.from(mContext).notify(R.id.mediaControlNotification, getCurrentNotification());
        }
    }

    private Notification.Action createNotificationAction() {
        final Intent intent = createIntentUponState(sMediaState);
        boolean isPlayAction = intent.getAction().equals(ACTION_RESUME);

        int icon = isPlayAction ? R.drawable.ic_media_play : R.drawable.ic_media_pause;
        String title = mContext.getString(isPlayAction ? R.string.media_play : R.string.media_pause);

        final PendingIntent pendingIntent = PendingIntent.getService(mContext, 1, intent, 0);

        //noinspection deprecation - The new constructor is only for API > 23
        return new Notification.Action.Builder(icon, title, pendingIntent).build();
    }

    /**
     * This method encapsulated UI logic. For PLAYING state, UI should display a PAUSE icon.
     * @param state The expected current state of MediaControlService
     * @return corresponding Intent to be used for Notification
     */
    @VisibleForTesting
    Intent createIntentUponState(State state) {
        String action = state.equals(State.PLAYING) ? ACTION_PAUSE : ACTION_RESUME;
        final Intent intent = new Intent(mContext, MediaControlService.class);
        intent.setAction(action);
        return intent;
    }

    private PendingIntent createContentIntent(Tab tab) {
        Intent intent = IntentHelper.getTabSwitchIntent(tab);
        return PendingIntent.getActivity(mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private PendingIntent createDeleteIntent() {
        Intent intent = new Intent(mContext, MediaControlService.class);
        intent.setAction(ACTION_STOP);
        return PendingIntent.getService(mContext, 1, intent, 0);
    }

    private Bitmap generateCoverArt(Tab tab) {
        final Bitmap favicon = tab.getFavicon();

        // If we do not have a favicon or if it's smaller than 72 pixels then just use the default icon.
        if (favicon == null || favicon.getWidth() < minCoverSize || favicon.getHeight() < minCoverSize) {
            // Use the launcher icon as fallback
            return BitmapFactory.decodeResource(mContext.getResources(), R.drawable.notification_media);
        }

        // Favicon should at least have half of the size of the cover
        int width = Math.max(favicon.getWidth(), coverSize / 2);
        int height = Math.max(favicon.getHeight(), coverSize / 2);

        final Bitmap coverArt = Bitmap.createBitmap(coverSize, coverSize, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(coverArt);
        canvas.drawColor(0xFF777777);

        int left = Math.max(0, (coverArt.getWidth() / 2) - (width / 2));
        int right = Math.min(coverSize, left + width);
        int top = Math.max(0, (coverArt.getHeight() / 2) - (height / 2));
        int bottom = Math.min(coverSize, top + height);

        final Paint paint = new Paint();
        paint.setAntiAlias(true);

        canvas.drawBitmap(favicon,
                new Rect(0, 0, favicon.getWidth(), favicon.getHeight()),
                new Rect(left, top, right, bottom),
                paint);

        return coverArt;
    }

    @SuppressLint("NewApi")
    private void startForegroundService() {
        Intent intent = new Intent(mContext, MediaControlService.class);

        if (AppConstants.Versions.preO) {
            mContext.startService(intent);
        } else {
            mContext.startForegroundService(intent);
        }
    }

    private void stopForegroundService() {
        mContext.stopService(new Intent(mContext, MediaControlService.class));
    }

    private void release() {
        if (!mInitialized) {
            return;
        }
        mInitialized = false;

        Log.d(LOGTAG, "release");
        if (!sMediaState.equals(State.STOPPED)) {
            setState(State.STOPPED);
        }
        PrefsHelper.removeObserver(mPrefsObserver);
        mHeadSetStateReceiver.unregisterReceiver(mContext);
        mSession.release();
    }

    private class HeadSetStateReceiver extends BroadcastReceiver {
        @CheckResult(suggest = "new HeadSetStateReceiver().registerReceiver(Context)")
        HeadSetStateReceiver registerReceiver(Context context) {
            IntentFilter intentFilter = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
            context.registerReceiver(this, intentFilter);
            return this;
        }

        void unregisterReceiver(Context context) {
            context.unregisterReceiver(HeadSetStateReceiver.this);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (isMediaPlaying()) {
                handleAction(ACTION_PAUSE);
            }
        }
    }
}
