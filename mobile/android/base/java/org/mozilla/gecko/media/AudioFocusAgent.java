package org.mozilla.gecko.media;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;

import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

public class AudioFocusAgent {
    private static final String LOGTAG = "AudioFocusAgent";

    private static Context mContext;
    private AudioManager mAudioManager;
    private OnAudioFocusChangeListener mAfChangeListener;

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

        mContext = context;
        mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);

        mAfChangeListener = new OnAudioFocusChangeListener() {
            public void onAudioFocusChange(int focusChange) {
                switch (focusChange) {
                    case AudioManager.AUDIOFOCUS_LOSS:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_LOSS");
                        notifyObservers("audioFocusChanged", "lostAudioFocus");
                        notifyMediaControlService(MediaControlService.ACTION_PAUSE_BY_AUDIO_FOCUS);
                        mAudioFocusState = State.LOST_FOCUS;
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_LOSS_TRANSIENT");
                        notifyObservers("audioFocusChanged", "lostAudioFocusTransiently");
                        notifyMediaControlService(MediaControlService.ACTION_PAUSE_BY_AUDIO_FOCUS);
                        mAudioFocusState = State.LOST_FOCUS_TRANSIENT;
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK");
                        notifyMediaControlService(MediaControlService.ACTION_START_AUDIO_DUCK);
                        mAudioFocusState = State.LOST_FOCUS_TRANSIENT_CAN_DUCK;
                        break;
                    case AudioManager.AUDIOFOCUS_GAIN:
                        if (mAudioFocusState.equals(State.LOST_FOCUS_TRANSIENT_CAN_DUCK)) {
                            Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_GAIN (from DUCKING)");
                            notifyMediaControlService(MediaControlService.ACTION_STOP_AUDIO_DUCK);
                        } else if (mAudioFocusState.equals(State.LOST_FOCUS_TRANSIENT)) {
                            Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_GAIN");
                            notifyObservers("audioFocusChanged", "gainAudioFocus");
                            notifyMediaControlService(MediaControlService.ACTION_RESUME_BY_AUDIO_FOCUS);
                        }
                        mAudioFocusState = State.OWN_FOCUS;
                        break;
                    default:
                }
            }
        };
        notifyMediaControlService(MediaControlService.ACTION_INIT);
    }

    @RobocopTarget
    public static AudioFocusAgent getInstance() {
        return AudioFocusAgent.SingletonHolder.INSTANCE;
    }

    private static class SingletonHolder {
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

    private void notifyMediaControlService(String action) {
        Intent intent = new Intent(mContext, MediaControlService.class);
        intent.setAction(action);
        mContext.startService(intent);
    }

    @VisibleForTesting
    public State getAudioFocusState() {
        return mAudioFocusState;
    }

    @VisibleForTesting
    public void changeAudioFocus(int focusChange) {
        mAfChangeListener.onAudioFocusChange(focusChange);
    }
}
