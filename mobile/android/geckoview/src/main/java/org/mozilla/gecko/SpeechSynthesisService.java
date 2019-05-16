/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil -*- */
/* vim: set ts=20 sts=4 et sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.os.Build;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.util.Log;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;

public class SpeechSynthesisService  {
    private static final String LOGTAG = "GeckoSpeechSynthesis";
    // Object type is used to make it easier to remove android.speech dependencies using Proguard.
    private static Object sTTS;

    @WrapForJNI(calledFrom = "gecko")
    public static void initSynth() {
        initSynthInternal();
    }

    // Extra internal method to make it easier to remove android.speech dependencies using Proguard.
    private static void initSynthInternal() {
        if (sTTS != null) {
            return;
        }

        final Context ctx = GeckoAppShell.getApplicationContext();

        sTTS = new TextToSpeech(ctx, new TextToSpeech.OnInitListener() {
            @Override
            public void onInit(final int status) {
                if (status != TextToSpeech.SUCCESS) {
                    Log.w(LOGTAG, "Failed to initialize TextToSpeech");
                    return;
                }

                setUtteranceListener();
                registerVoicesByLocale();
            }
        });
    }

    private static TextToSpeech getTTS() {
        return (TextToSpeech) sTTS;
    }

    private static void registerVoicesByLocale() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                TextToSpeech tss = getTTS();
                Locale defaultLocale = Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2
                        ? tss.getDefaultLanguage()
                        : tss.getLanguage();
                for (Locale locale : getAvailableLanguages()) {
                    final Set<String> features = tss.getFeatures(locale);
                    boolean isLocal = features != null && features.contains(TextToSpeech.Engine.KEY_FEATURE_EMBEDDED_SYNTHESIS);
                    String localeStr = locale.toString();
                    registerVoice("moz-tts:android:" + localeStr, locale.getDisplayName(), localeStr.replace("_", "-"), !isLocal, defaultLocale == locale);
                }
                doneRegisteringVoices();
            }
        });
    }

    private static Set<Locale> getAvailableLanguages() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // While this method was introduced in 21, it seems that it
            // has not been implemented in the speech service side until 23.
            return getTTS().getAvailableLanguages();
        }
        Set<Locale> locales = new HashSet<Locale>();
        for (Locale locale : Locale.getAvailableLocales()) {
            if (locale.getVariant().isEmpty() && getTTS().isLanguageAvailable(locale) > 0) {
                locales.add(locale);
            }
        }

        return locales;
    }

    @WrapForJNI(dispatchTo = "gecko")
    private static native void registerVoice(String uri, String name, String locale, boolean isNetwork, boolean isDefault);

    @WrapForJNI(dispatchTo = "gecko")
    private static native void doneRegisteringVoices();

    @WrapForJNI(calledFrom = "gecko")
    public static String speak(final String uri, final String text, final float rate,
                               final float pitch, final float volume) {
        AtomicBoolean result = new AtomicBoolean(false);
        final String utteranceId = UUID.randomUUID().toString();
        speakInternal(uri, text, rate, pitch, volume, utteranceId, result);
        return result.get() ? utteranceId : null;
    }

    // Extra internal method to make it easier to remove android.speech dependencies using Proguard.
    private static void speakInternal(final String uri, final String text, final float rate,
                                      final float pitch, final float volume, final String utteranceId, final AtomicBoolean result) {
        if (sTTS == null) {
            Log.w(LOGTAG, "TextToSpeech is not initialized");
            return;
        }

        HashMap<String, String> params = new HashMap<String, String>();
        params.put(TextToSpeech.Engine.KEY_PARAM_VOLUME, Float.toString(volume));
        params.put(TextToSpeech.Engine.KEY_PARAM_UTTERANCE_ID, utteranceId);
        TextToSpeech tss = (TextToSpeech) sTTS;
        tss.setLanguage(new Locale(uri.substring("moz-tts:android:".length())));
        tss.setSpeechRate(rate);
        tss.setPitch(pitch);
        int speakRes = tss.speak(text, TextToSpeech.QUEUE_FLUSH, params);
        result.set(speakRes == TextToSpeech.SUCCESS);
    }

    private static void setUtteranceListener() {
        if (sTTS == null) {
            Log.w(LOGTAG, "TextToSpeech is not initialized");
            return;
        }

        getTTS().setOnUtteranceProgressListener(new UtteranceProgressListener() {
            @Override
            public void onDone(final String utteranceId) {
                dispatchEnd(utteranceId);
            }

            @Override
            public void onError(final String utteranceId) {
                dispatchError(utteranceId);
            }

            @Override
            public void onStart(final String utteranceId) {
                dispatchStart(utteranceId);
            }

            @Override
            public void onStop(final String utteranceId, final boolean interrupted) {
                if (interrupted) {
                    dispatchEnd(utteranceId);
                } else {
                    // utterance isn't started yet.
                    dispatchError(utteranceId);
                }
            }

            public void onRangeStart(final String utteranceId, final int start, final int end,
                                     final int frame) {
                dispatchBoundary(utteranceId, start, end);
            }
        });
    }

    @WrapForJNI(dispatchTo = "gecko")
    private static native void dispatchStart(String utteranceId);

    @WrapForJNI(dispatchTo = "gecko")
    private static native void dispatchEnd(String utteranceId);

    @WrapForJNI(dispatchTo = "gecko")
    private static native void dispatchError(String utteranceId);

    @WrapForJNI(dispatchTo = "gecko")
    private static native void dispatchBoundary(String utteranceId, int start, int end);

    @WrapForJNI(calledFrom = "gecko")
    public static void stop() {
        stopInternal();
    }

    // Extra internal method to make it easier to remove android.speech dependencies using Proguard.
    private static void stopInternal() {
        if (sTTS == null) {
            Log.w(LOGTAG, "TextToSpeech is not initialized");
            return;
        }

        getTTS().stop();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            // Android M has onStop method.  If Android L or above, dispatch
            // event
            dispatchEnd(null);
        }
    }
}
