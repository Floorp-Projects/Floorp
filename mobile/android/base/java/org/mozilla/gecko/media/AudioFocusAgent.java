package org.mozilla.gecko.media;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;

import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.util.Log;

public class AudioFocusAgent {
    private static final String LOGTAG = "AudioFocusAgent";

    private static Context mContext;
    private AudioManager mAudioManager;
    private OnAudioFocusChangeListener mAfChangeListener;

    public static final String OWN_FOCUS = "own_focus";
    public static final String LOST_FOCUS = "lost_focus";
    public static final String LOST_FOCUS_TRANSIENT = "lost_focus_transient";

    private String mAudioFocusState = LOST_FOCUS;

    @WrapForJNI
    public static void notifyStartedPlaying() {
        if (!isAttachedToContext()) {
            return;
        }
        Log.d(LOGTAG, "NotifyStartedPlaying");
        AudioFocusAgent.getInstance().requestAudioFocusIfNeeded();
    }

    @WrapForJNI
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
                        notifyObservers("AudioFocusChanged", "lostAudioFocus");
                        notifyMediaControlService(MediaControlService.ACTION_PAUSE);
                        mAudioFocusState = LOST_FOCUS;
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_LOSS_TRANSIENT");
                        notifyObservers("AudioFocusChanged", "lostAudioFocusTransiently");
                        notifyMediaControlService(MediaControlService.ACTION_PAUSE);
                        mAudioFocusState = LOST_FOCUS_TRANSIENT;
                        break;
                    case AudioManager.AUDIOFOCUS_GAIN:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_GAIN");
                        notifyObservers("AudioFocusChanged", "gainAudioFocus");
                        notifyMediaControlService(MediaControlService.ACTION_PLAY);
                        mAudioFocusState = OWN_FOCUS;
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
        if (mAudioFocusState.equals(OWN_FOCUS)) {
            return;
        }

        int result = mAudioManager.requestAudioFocus(mAfChangeListener,
                                                     AudioManager.STREAM_MUSIC,
                                                     AudioManager.AUDIOFOCUS_GAIN);

        String focusMsg = (result == AudioManager.AUDIOFOCUS_GAIN) ?
            "AudioFocus request granted" : "AudioFoucs request failed";
        Log.d(LOGTAG, focusMsg);
        if (result == AudioManager.AUDIOFOCUS_GAIN) {
            mAudioFocusState = OWN_FOCUS;
            notifyMediaControlService(MediaControlService.ACTION_START);
        }
    }

    private void abandonAudioFocusIfNeeded() {
        if (!mAudioFocusState.equals(OWN_FOCUS)) {
            return;
        }

        Log.d(LOGTAG, "Abandon AudioFocus");
        mAudioManager.abandonAudioFocus(mAfChangeListener);
        mAudioFocusState = LOST_FOCUS;
        notifyMediaControlService(MediaControlService.ACTION_STOP);
    }

    private void notifyMediaControlService(String action) {
        Intent intent = new Intent(mContext, MediaControlService.class);
        intent.setAction(action);
        mContext.startService(intent);
    }
}