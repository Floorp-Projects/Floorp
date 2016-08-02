package org.mozilla.gecko.media;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.media.session.MediaController;
import android.media.session.MediaSession;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.support.v4.app.NotificationManagerCompat;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.ThreadUtils;

import java.lang.ref.WeakReference;

public class MediaControlService extends Service implements Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "MediaControlService";

    public static final String ACTION_START          = "action_start";
    public static final String ACTION_PLAY           = "action_play";
    public static final String ACTION_PAUSE          = "action_pause";
    public static final String ACTION_STOP           = "action_stop";
    public static final String ACTION_REMOVE_CONTROL = "action_remove_control";

    private static final int MEDIA_CONTROL_ID = 1;
    private static final String MEDIA_CONTROL_PREF = "dom.audiochannel.mediaControl";

    private String mActionState = ACTION_STOP;

    private MediaSession mSession;
    private MediaController mController;

    private PrefsHelper.PrefHandler mPrefsObserver;
    private final String[] mPrefs = { MEDIA_CONTROL_PREF };

    private boolean mIsInitMediaSession = false;
    private boolean mIsMediaControlPrefOn = true;

    private static WeakReference<Tab> mTabReference = null;

    private int coverSize;

    @Override
    public void onCreate() {
        mTabReference = new WeakReference<>(null);

        getGeckoPreference();
        initMediaSession();

        coverSize = (int) getResources().getDimension(R.dimen.notification_media_cover);

        Tabs.registerOnTabsChangedListener(this);
    }

    @Override
    public void onDestroy() {
        notifyControlInterfaceChanged(ACTION_REMOVE_CONTROL);
        PrefsHelper.removeObserver(mPrefsObserver);

        Tabs.unregisterOnTabsChangedListener(this);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        handleIntent(intent);
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        mSession.release();
        return super.onUnbind(intent);
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        stopSelf();
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        if (!mIsInitMediaSession) {
            return;
        }

        switch (msg) {
            case AUDIO_PLAYING_CHANGE:
                if (tab == mTabReference.get()) {
                    return;
                }

                mTabReference = new WeakReference<>(tab);
                notifyControlInterfaceChanged(ACTION_PAUSE);
                break;

            case CLOSED:
                final Tab playingTab = mTabReference.get();
                if (playingTab == null || playingTab == tab) {
                    // The playing tab disappeared or was closed. Remove the controls and stop the service.
                    notifyControlInterfaceChanged(ACTION_REMOVE_CONTROL);
                }
                break;
        }
    }

    private boolean isAndroidVersionLollopopOrHigher() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
    }

    private void handleIntent(Intent intent) {
        if (intent == null || intent.getAction() == null ||
           !mIsInitMediaSession) {
            return;
        }

        Log.d(LOGTAG, "HandleIntent, action = " + intent.getAction() + ", actionState = " + mActionState);
        switch (intent.getAction()) {
            case ACTION_START :
                mController.getTransportControls().sendCustomAction(ACTION_START, null);
                break;
            case ACTION_PLAY :
                mController.getTransportControls().play();
                break;
            case ACTION_PAUSE :
                mController.getTransportControls().pause();
                break;
            case ACTION_STOP :
                if (!mActionState.equals(ACTION_PLAY)) {
                    return;
                }
                mController.getTransportControls().stop();
                break;
            case ACTION_REMOVE_CONTROL :
                mController.getTransportControls().stop();
                break;
        }
    }

    private void getGeckoPreference() {
        mPrefsObserver = new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean value) {
                if (pref.equals(MEDIA_CONTROL_PREF)) {
                    mIsMediaControlPrefOn = value;

                    // If media is playing, we just need to create or remove
                    // the media control interface.
                    if (mActionState.equals(ACTION_PLAY)) {
                        notifyControlInterfaceChanged(mIsMediaControlPrefOn ?
                            ACTION_PAUSE : ACTION_REMOVE_CONTROL);
                    }

                    // If turn off pref during pausing, except removing media
                    // interface, we also need to stop the service and notify
                    // gecko about that.
                    if (mActionState.equals(ACTION_PAUSE) &&
                        !mIsMediaControlPrefOn) {
                        Intent intent = new Intent(getApplicationContext(), MediaControlService.class);
                        intent.setAction(ACTION_REMOVE_CONTROL);
                        handleIntent(intent);
                    }
                }
            }
        };
        PrefsHelper.addObserver(mPrefs, mPrefsObserver);
    }

    private void initMediaSession() {
        if (!isAndroidVersionLollopopOrHigher() || mIsInitMediaSession) {
            return;
        }

        // Android MediaSession is introduced since version L.
        mSession = new MediaSession(getApplicationContext(),
                                    "fennec media session");
        mController = new MediaController(getApplicationContext(),
                                          mSession.getSessionToken());

        mSession.setCallback(new MediaSession.Callback() {
            @Override
            public void onCustomAction(String action, Bundle extras) {
                if (action.equals(ACTION_START)) {
                    Log.d(LOGTAG, "Controller, onStart");
                    notifyControlInterfaceChanged(ACTION_PAUSE);
                    mActionState = ACTION_PLAY;
                }
            }

            @Override
            public void onPlay() {
                Log.d(LOGTAG, "Controller, onPlay");
                super.onPlay();
                notifyControlInterfaceChanged(ACTION_PAUSE);
                notifyObservers("MediaControl", "resumeMedia");
                mActionState = ACTION_PLAY;
            }

            @Override
            public void onPause() {
                Log.d(LOGTAG, "Controller, onPause");
                super.onPause();
                notifyControlInterfaceChanged(ACTION_PLAY);
                notifyObservers("MediaControl", "mediaControlPaused");
                mActionState = ACTION_PAUSE;
            }

            @Override
            public void onStop() {
                Log.d(LOGTAG, "Controller, onStop");
                super.onStop();
                notifyControlInterfaceChanged(ACTION_STOP);
                notifyObservers("MediaControl", "mediaControlStopped");
                mActionState = ACTION_STOP;
                stopSelf();
            }
        });
        mIsInitMediaSession = true;
    }

    private void notifyObservers(String topic, String data) {
        GeckoAppShell.notifyObservers(topic, data);
    }

    private boolean isNeedToRemoveControlInterface(String action) {
        return (action.equals(ACTION_STOP) ||
                action.equals(ACTION_REMOVE_CONTROL));
    }

    private void notifyControlInterfaceChanged(final String action) {
        Log.d(LOGTAG, "notifyControlInterfaceChanged, action = " + action);

        if (isNeedToRemoveControlInterface(action)) {
            NotificationManagerCompat.from(this).cancel(MEDIA_CONTROL_ID);
            return;
        }

        if (!mIsMediaControlPrefOn) {
            return;
        }

        // TODO : remove this checking when the media control is ready to ship,
        // see bug1290836.
        if (!AppConstants.NIGHTLY_BUILD) {
            return;
        }

        final Tab tab = mTabReference.get();

        if (tab == null) {
            return;
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                updateNotification(tab, action);
            }
        });
    }

    private void updateNotification(Tab tab, String action) {
        ThreadUtils.assertNotOnUiThread();

        final Notification.MediaStyle style = new Notification.MediaStyle();
        style.setShowActionsInCompactView(0);

        final boolean isMediaPlaying = action.equals(ACTION_PAUSE);
        final int visibility = tab.isPrivate() ?
            Notification.VISIBILITY_PRIVATE : Notification.VISIBILITY_PUBLIC;

        final Notification notification = new Notification.Builder(this)
            .setSmallIcon(R.drawable.flat_icon)
            .setLargeIcon(generateCoverArt(tab))
            .setContentTitle(tab.getTitle())
            .setContentText(tab.getURL())
            .setContentIntent(createContentIntent())
            .setDeleteIntent(createDeleteIntent())
            .setStyle(style)
            .addAction(createNotificationAction(action))
            .setOngoing(isMediaPlaying)
            .setShowWhen(false)
            .setWhen(0)
            .setVisibility(visibility)
            .build();

        if (isMediaPlaying) {
            startForeground(MEDIA_CONTROL_ID, notification);
        } else {
            stopForeground(false);
            NotificationManagerCompat.from(this)
                .notify(MEDIA_CONTROL_ID, notification);
        }
    }

    private Notification.Action createNotificationAction(String action) {
        boolean isPlayAction = action.equals(ACTION_PLAY);

        int icon = isPlayAction ? R.drawable.ic_media_play : R.drawable.ic_media_pause;
        String title = getString(isPlayAction ? R.string.media_play : R.string.media_pause);

        final Intent intent = new Intent(getApplicationContext(), MediaControlService.class);
        intent.setAction(action);
        final PendingIntent pendingIntent = PendingIntent.getService(getApplicationContext(), 1, intent, 0);

        //noinspection deprecation - The new constructor is only for API > 23
        return new Notification.Action.Builder(icon, title, pendingIntent).build();
    }

    private PendingIntent createContentIntent() {
        Intent intent = new Intent(getApplicationContext(), BrowserApp.class);
        return PendingIntent.getActivity(getApplicationContext(), 0, intent, 0);
    }

    private PendingIntent createDeleteIntent() {
        Intent intent = new Intent(getApplicationContext(), MediaControlService.class);
        intent.setAction(ACTION_REMOVE_CONTROL);
        return  PendingIntent.getService(getApplicationContext(), 1, intent, 0);
    }

    private Bitmap generateCoverArt(Tab tab) {
        final Bitmap favicon = tab.getFavicon();

        // If we do not have a favicon or if it's smaller than 72 pixels then just use the default icon.
        if (favicon == null || favicon.getWidth() < 72 || favicon.getHeight() < 72) {
            // Use the launcher icon as fallback
            return BitmapFactory.decodeResource(getResources(), R.drawable.notification_media);
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
}