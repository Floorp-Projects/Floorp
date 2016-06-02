package org.mozilla.gecko.media;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.PrefsHelper;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.media.session.MediaController;
import android.media.session.MediaSession;
import android.media.session.MediaSessionManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;
import android.R;

public class MediaControlService extends Service {
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

    @Override
    public void onCreate() {
        getGeckoPreference();
        initMediaSession();
    }

    @Override
    public void onDestroy() {
        notifyControlInterfaceChanged(ACTION_REMOVE_CONTROL);
        PrefsHelper.removeObserver(mPrefsObserver);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        handleIntent(intent);
        return super.onStartCommand(intent, flags, startId);
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

    private boolean isAndroidVersionLollopopOrHigher() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
    }

    private void handleIntent(Intent intent) {
        if(intent == null || intent.getAction() == null ||
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

    private void notifyControlInterfaceChanged(String action) {
        Log.d(LOGTAG, "notifyControlInterfaceChanged, action = " + action);
        NotificationManager notificationManager = (NotificationManager)
            getSystemService(Context.NOTIFICATION_SERVICE);

        if (isNeedToRemoveControlInterface(action)) {
            notificationManager.cancel(MEDIA_CONTROL_ID);
            return;
        }

        if (!mIsMediaControlPrefOn) {
            return;
        }

        notificationManager.notify(MEDIA_CONTROL_ID, getNotification(action));
    }

    private Notification getNotification(String action) {
        // TODO : use website name, content and favicon in bug1264901.
        return new Notification.Builder(this)
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setContentTitle("Media Title")
            .setContentText("Media Artist")
            .setDeleteIntent(getDeletePendingIntent())
            .setContentIntent(getClickPendingIntent())
            .setStyle(getMediaStyle())
            .addAction(getAction(action))
            .setOngoing(action.equals(ACTION_PAUSE))
            .build();
    }

    private Notification.Action getAction(String action) {
        int icon = getActionIcon(action);
        String title = getActionTitle(action);

        Intent intent = new Intent(getApplicationContext(), MediaControlService.class);
        intent.setAction(action);
        PendingIntent pendingIntent = PendingIntent.getService(getApplicationContext(), 1, intent, 0);
        return new Notification.Action.Builder(icon, title, pendingIntent).build();
    }

    private int getActionIcon(String action) {
        switch (action) {
            case ACTION_PLAY :
                return android.R.drawable.ic_media_play;
            case ACTION_PAUSE :
                return android.R.drawable.ic_media_pause;
            default:
                return 0;
        }
    }

    private String getActionTitle(String action) {
        switch (action) {
            case ACTION_PLAY :
                return "Play";
            case ACTION_PAUSE :
                return "Pause";
            default:
                return null;
        }
    }

    private PendingIntent getDeletePendingIntent() {
        Intent intent = new Intent(getApplicationContext(), MediaControlService.class);
        intent.setAction(ACTION_REMOVE_CONTROL);
        return PendingIntent.getService(getApplicationContext(), 1, intent, 0);
    }

    private PendingIntent getClickPendingIntent() {
        Intent intent = new Intent(getApplicationContext(), BrowserApp.class);
        return PendingIntent.getActivity(getApplicationContext(), 0, intent, 0);
    }

    private Notification.MediaStyle getMediaStyle() {
        Notification.MediaStyle style = new Notification.MediaStyle();
        style.setShowActionsInCompactView(0);
        return style;
    }
}