package org.mozilla.gecko.media;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;

import android.content.Context;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.util.Log;

public class AudioFocusAgent {
    private static final String LOGTAG = "AudioFocusAgent";

    private static Context mContext;
    private AudioManager mAudioManager;
    private OnAudioFocusChangeListener mAfChangeListener;
    private int mAudibleElementCounts;

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
                        // TODO : to dispatch audio-stop from gecko to trigger abandonAudioFocusIfNeeded
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_LOSS_TRANSIENT");
                        notifyObservers("AudioFocusChanged", "lostAudioFocusTransiently");
                        break;
                    case AudioManager.AUDIOFOCUS_GAIN:
                        Log.d(LOGTAG, "onAudioFocusChange, AUDIOFOCUS_GAIN");
                        notifyObservers("AudioFocusChanged", "gainAudioFocus");
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

    private AudioFocusAgent() {
        mAudibleElementCounts = 0;
    }

    private void requestAudioFocusIfNeeded() {
        if (!isFirstAudibleElement()) {
            return;
        }

        int result = mAudioManager.requestAudioFocus(mAfChangeListener,
                                                     AudioManager.STREAM_MUSIC,
                                                     AudioManager.AUDIOFOCUS_GAIN);

        String focusMsg = (result == AudioManager.AUDIOFOCUS_GAIN) ?
            "AudioFocus request granted" : "AudioFoucs request failed";
        Log.d(LOGTAG, focusMsg);
        // TODO : Enable media control when get the AudioFocus, see bug1240423.
    }

    private void abandonAudioFocusIfNeeded() {
        if (!isLastAudibleElement()) {
            return;
        }

        Log.d(LOGTAG, "Abandon AudioFocus");
        mAudioManager.abandonAudioFocus(mAfChangeListener);
    }

    private boolean isFirstAudibleElement() {
        return (++mAudibleElementCounts == 1);
    }

    private boolean isLastAudibleElement() {
        return (--mAudibleElementCounts == 0);
    }
}