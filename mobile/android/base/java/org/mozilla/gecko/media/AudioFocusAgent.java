package org.mozilla.gecko.media;

import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import java.lang.ref.WeakReference;

import static org.mozilla.gecko.AppConstants.Versions;

public class AudioFocusAgent implements Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "AudioFocusAgent";

    // We're referencing the *application* context, so this is in fact okay.
    @SuppressLint("StaticFieldLeak")
    private static Context mContext;
    private AudioManager mAudioManager;
    private OnAudioFocusChangeListener mAfChangeListener;

    private WeakReference<Tab> mTabReference = new WeakReference<>(null);

    public enum State {
        OWN_FOCUS,
        LOST_FOCUS,
        LOST_FOCUS_TRANSIENT,
        LOST_FOCUS_TRANSIENT_CAN_DUCK
    }

    private State mAudioFocusState = State.LOST_FOCUS;

    @WrapForJNI(calledFrom = "gecko")
    public static void notifyStartedPlaying() {
        if (!isAttachedToContext()) {
            return;
        }
        Log.d(LOGTAG, "NotifyStartedPlaying");
        AudioFocusAgent.getInstance().requestAudioFocusIfNeeded();
    }

    @WrapForJNI(calledFrom = "gecko")
    public static void notifyStoppedPlaying() {
        if (!isAttachedToContext()) {
            return;
        }
        Log.d(LOGTAG, "NotifyStoppedPlaying");
        AudioFocusAgent.getInstance().abandonAudioFocusIfNeeded();
    }

    public synchronized void attachToContext(Context context) {
        if (isAttachedToContext()) {
            return;
        }

        mContext = context.getApplicationContext();
        mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        Tabs.registerOnTabsChangedListener(this);

        mAfChangeListener = new OnAudioFocusChangeListener() {
            public void onAudioFocusChange(int focusChange) {
                switch (focusChange) {
                    case AudioManager.AUDIOFOCUS_LOSS:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_LOSS");
                        mAudioFocusState = State.LOST_FOCUS;
                        notifyObservers("audioFocusChanged", "lostAudioFocus");
                        notifyMediaControlService(MediaControlService.ACTION_PAUSE_BY_AUDIO_FOCUS);
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_LOSS_TRANSIENT");
                        mAudioFocusState = State.LOST_FOCUS_TRANSIENT;
                        notifyObservers("audioFocusChanged", "lostAudioFocusTransiently");
                        notifyMediaControlService(MediaControlService.ACTION_PAUSE_BY_AUDIO_FOCUS);
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK");
                        mAudioFocusState = State.LOST_FOCUS_TRANSIENT_CAN_DUCK;
                        notifyMediaControlService(MediaControlService.ACTION_START_AUDIO_DUCK);
                        break;
                    case AudioManager.AUDIOFOCUS_GAIN:
                        State state = mAudioFocusState;
                        mAudioFocusState = State.OWN_FOCUS;
                        if (state.equals(State.LOST_FOCUS_TRANSIENT_CAN_DUCK)) {
                            Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_GAIN (from DUCKING)");
                            notifyMediaControlService(MediaControlService.ACTION_STOP_AUDIO_DUCK);
                        } else if (state.equals(State.LOST_FOCUS_TRANSIENT)) {
                            Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_GAIN");
                            notifyObservers("audioFocusChanged", "gainAudioFocus");
                            notifyMediaControlService(MediaControlService.ACTION_RESUME_BY_AUDIO_FOCUS);
                        }
                        break;
                    default:
                }
            }
        };
    }

    @RobocopTarget
    public static AudioFocusAgent getInstance() {
        return AudioFocusAgent.SingletonHolder.INSTANCE;
    }

    private static class SingletonHolder {
        // We're referencing the *application* context, so this is in fact okay.
        @SuppressLint("StaticFieldLeak")
        private static final AudioFocusAgent INSTANCE = new AudioFocusAgent();
    }

    private static boolean isAttachedToContext() {
        return (mContext != null);
    }

    private void notifyObservers(String topic, String data) {
        GeckoAppShell.notifyObservers(topic, data);
    }

    private AudioFocusAgent() {}

    private void requestAudioFocusIfNeeded() {
        if (mAudioFocusState.equals(State.OWN_FOCUS)) {
            return;
        }

        int result = mAudioManager.requestAudioFocus(mAfChangeListener,
                                                     AudioManager.STREAM_MUSIC,
                                                     AudioManager.AUDIOFOCUS_GAIN);

        String focusMsg = (result == AudioManager.AUDIOFOCUS_GAIN) ?
            "AudioFocus request granted" : "AudioFoucs request failed";
        Log.d(LOGTAG, focusMsg);
        if (result == AudioManager.AUDIOFOCUS_GAIN) {
            mAudioFocusState = State.OWN_FOCUS;
        }
    }

    private void abandonAudioFocusIfNeeded() {
        if (!mAudioFocusState.equals(State.OWN_FOCUS)) {
            return;
        }

        Log.d(LOGTAG, "Abandon AudioFocus");
        mAudioManager.abandonAudioFocus(mAfChangeListener);
        mAudioFocusState = State.LOST_FOCUS;
    }

    /* package */ Tab getActiveMediaTab() {
        return mTabReference.get();
    }

    /* package */ void clearActiveMediaTab() {
        mTabReference = new WeakReference<>(null);
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        if (!isAttachedToContext()) {
            return;
        }

        final Tab playingTab = mTabReference.get();
        switch (msg) {
            case MEDIA_PLAYING_CHANGE:
                // The 'MEDIA_PLAYING_CHANGE' would only be received when the
                // media starts or ends.
                if (playingTab != tab && tab.isMediaPlaying()) {
                    mTabReference = new WeakReference<>(tab);
                    notifyMediaControlService(MediaControlService.ACTION_TAB_STATE_PLAYING);
                } else if (playingTab == tab) {
                    mTabReference = new WeakReference<>(tab.isMediaPlaying() ? tab : null);
                    final String action = tab.isMediaPlaying()
                            ? MediaControlService.ACTION_TAB_STATE_PLAYING
                            : MediaControlService.ACTION_TAB_STATE_STOPPED;
                    notifyMediaControlService(action);
                }
                break;
            case MEDIA_PLAYING_RESUME:
                // user resume the paused-by-control media from page so that we
                // should make the control interface consistent.
                if (playingTab == tab) {
                    notifyMediaControlService(MediaControlService.ACTION_TAB_STATE_RESUMED);
                }
                break;
            case CLOSED:
                if (playingTab == null || playingTab == tab) {
                    // Remove the controls when the playing tab disappeared or was closed.
                    notifyMediaControlService(MediaControlService.ACTION_TAB_STATE_STOPPED);
                }
                break;
            case FAVICON:
                if (playingTab == tab) {
                    notifyMediaControlService(MediaControlService.ACTION_TAB_STATE_FAVICON);
                }
                break;
        }
    }

    private void notifyMediaControlService(String action) {
        if (Versions.preLollipop) {
            // The notification only works from Lollipop onwards (at least until we try using
            // the support library version), so there's no point in starting the service.
            return;
        }

        Intent intent = new Intent(mContext, MediaControlService.class);
        intent.setAction(action);
        mContext.startService(intent);
    }

    @VisibleForTesting
    @RobocopTarget
    public State getAudioFocusState() {
        return mAudioFocusState;
    }

    @VisibleForTesting
    @RobocopTarget
    public void changeAudioFocus(int focusChange) {
        mAfChangeListener.onAudioFocusChange(focusChange);
    }
}
