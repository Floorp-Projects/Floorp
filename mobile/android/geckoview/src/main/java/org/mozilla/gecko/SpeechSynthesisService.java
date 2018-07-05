/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil -*- */
/* vim: set ts=20 sts=4 et sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.os.Build;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.util.Log;

import java.lang.System;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;

public class SpeechSynthesisService  {
    private static final String LOGTAG = "GeckoSpeechSynthesis";
    private static TextToSpeech sTTS;

    @WrapForJNI(calledFrom = "gecko")
    public static void initSynth() {
        if (sTTS != null) {
            return;
        }

        final Context ctx = GeckoAppShell.getApplicationContext();

        sTTS = new TextToSpeech(ctx, new TextToSpeech.OnInitListener() {
            @Override
            public void onInit(int status) {
                if (status != TextToSpeech.SUCCESS) {
                    Log.w(LOGTAG, "Failed to initialize TextToSpeech");
                    return;
                }

                setUtteranceListener();
                registerVoicesByLocale();
            }
        });
    }

    private static void registerVoicesByLocale() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                Locale defaultLocale = sTTS.getDefaultLanguage();
                for (Locale locale : getAvailableLanguages()) {
                    final Set<String> features = sTTS.getFeatures(locale);
                    boolean isLocal = features.contains(TextToSpeech.Engine.KEY_FEATURE_EMBEDDED_SYNTHESIS);
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
            return sTTS.getAvailableLanguages();
        }
        Set<Locale> locales = new HashSet<Locale>();
        for (Locale locale : Locale.getAvailableLocales()) {
            if (locale.getVariant().isEmpty() && sTTS.isLanguageAvailable(locale) > 0) {
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
    public static String speak(final String uri, final String text, float rate, float pitch, float volume) {
        HashMap<String, String> params = new HashMap<String, String>();
        final String utteranceId = UUID.randomUUID().toString();
        params.put(TextToSpeech.Engine.KEY_PARAM_VOLUME, Float.toString(volume));
        params.put(TextToSpeech.Engine.KEY_PARAM_UTTERANCE_ID, utteranceId);
        sTTS.setLanguage(new Locale(uri.substring("moz-tts:android:".length())));
        sTTS.setSpeechRate(rate);
        sTTS.setPitch(pitch);
        int result = sTTS.speak(text, TextToSpeech.QUEUE_FLUSH, params);
        if (result != TextToSpeech.SUCCESS) {
            return null;
        }

        return utteranceId;
    }

    private static void setUtteranceListener() {
        sTTS.setOnUtteranceProgressListener(new UtteranceProgressListener() {
            @Override
            public void onDone(String utteranceId) {
                dispatchEnd(utteranceId);
            }

            @Override
            public void onError(String utteranceId) {
                dispatchError(utteranceId);
            }

            @Override
            public void onStart(String utteranceId) {
                dispatchStart(utteranceId);
            }

            @Override
            public void onStop(String utteranceId, boolean interrupted) {
                if (interrupted) {
                    dispatchEnd(utteranceId);
                } else {
                    // utterance isn't started yet.
                    dispatchError(utteranceId);
                }
            }

            public void onRangeStart (String utteranceId, int start, int end, int frame) {
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
        sTTS.stop();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            // Android M has onStop method.  If Android L or above, dispatch
            // event
            dispatchEnd(null);
        }
    }
}
