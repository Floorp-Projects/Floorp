/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;

import android.support.v4.util.SimpleArrayMap;
import android.util.Log;
import android.util.SparseArray;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;

/**
 * Helper class to get/set gecko prefs.
 */
public final class PrefsHelper {
    private static final String LOGTAG = "GeckoPrefsHelper";

    // Map pref name to ArrayList for multiple observers or PrefHandler for single observer.
    private static final SimpleArrayMap<String, Object> OBSERVERS = new SimpleArrayMap<>();
    private static final HashSet<String> INT_TO_STRING_PREFS = new HashSet<>(8);
    private static final HashSet<String> INT_TO_BOOL_PREFS = new HashSet<>(2);

    static {
        INT_TO_STRING_PREFS.add("browser.chrome.titlebarMode");
        INT_TO_STRING_PREFS.add("network.cookie.cookieBehavior");
        INT_TO_STRING_PREFS.add("font.size.inflation.minTwips");
        INT_TO_STRING_PREFS.add("home.sync.updateMode");
        INT_TO_STRING_PREFS.add("browser.image_blocking");
        INT_TO_BOOL_PREFS.add("browser.display.use_document_fonts");
    }

    @WrapForJNI
    private static final int PREF_INVALID = -1;
    @WrapForJNI
    private static final int PREF_FINISH = 0;
    @WrapForJNI
    private static final int PREF_BOOL = 1;
    @WrapForJNI
    private static final int PREF_INT = 2;
    @WrapForJNI
    private static final int PREF_STRING = 3;

    @WrapForJNI(stubName = "GetPrefs", dispatchTo = "gecko")
    private static native void nativeGetPrefs(String[] prefNames, PrefHandler handler);
    @WrapForJNI(stubName = "SetPref", dispatchTo = "gecko")
    private static native void nativeSetPref(String prefName, boolean flush, int type,
                                             boolean boolVal, int intVal, String strVal);
    @WrapForJNI(stubName = "AddObserver", dispatchTo = "gecko")
    private static native void nativeAddObserver(String[] prefNames, PrefHandler handler,
                                                 String[] prefsToObserve);
    @WrapForJNI(stubName = "RemoveObserver", dispatchTo = "gecko")
    private static native void nativeRemoveObserver(String[] prefToUnobserve);

    @RobocopTarget
    public static void getPrefs(final String[] prefNames, final PrefHandler callback) {
        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            nativeGetPrefs(prefNames, callback);
        } else {
            GeckoThread.queueNativeCallUntil(
                    GeckoThread.State.PROFILE_READY, PrefsHelper.class, "nativeGetPrefs",
                    String[].class, prefNames, PrefHandler.class, callback);
        }
    }

    public static void getPref(final String prefName, final PrefHandler callback) {
        getPrefs(new String[] { prefName }, callback);
    }

    public static void getPrefs(final ArrayList<String> prefNames, final PrefHandler callback) {
        getPrefs(prefNames.toArray(new String[prefNames.size()]), callback);
    }

    @RobocopTarget
    public static void setPref(final String pref, final Object value, final boolean flush) {
        final int type;
        boolean boolVal = false;
        int intVal = 0;
        String strVal = null;

        if (INT_TO_STRING_PREFS.contains(pref)) {
            // When sending to Java, we normalized special preferences that use integers
            // and strings to represent booleans. Here, we convert them back to their
            // actual types so we can store them.
            type = PREF_INT;
            intVal = Integer.parseInt(String.valueOf(value));
        } else if (INT_TO_BOOL_PREFS.contains(pref)) {
            type = PREF_INT;
            intVal = (Boolean) value ? 1 : 0;
        } else if (value instanceof Boolean) {
            type = PREF_BOOL;
            boolVal = (Boolean) value;
        } else if (value instanceof Integer) {
            type = PREF_INT;
            intVal = (Integer) value;
        } else {
            type = PREF_STRING;
            strVal = String.valueOf(value);
        }

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            nativeSetPref(pref, flush, type, boolVal, intVal, strVal);
        } else {
            GeckoThread.queueNativeCallUntil(
                    GeckoThread.State.PROFILE_READY, PrefsHelper.class, "nativeSetPref",
                    String.class, pref, flush, type, boolVal, intVal, String.class, strVal);
        }
    }

    public static void setPref(final String pref, final Object value) {
        setPref(pref, value, /* flush */ false);
    }

    @RobocopTarget
    public synchronized static void addObserver(final String[] prefNames,
                                                final PrefHandler handler) {
        List<String> prefsToObserve = null;

        for (String pref : prefNames) {
            final Object existing = OBSERVERS.get(pref);

            if (existing == null) {
                // Not observing yet, so add observer.
                if (prefsToObserve == null) {
                    prefsToObserve = new ArrayList<>(prefNames.length);
                }
                prefsToObserve.add(pref);
                OBSERVERS.put(pref, handler);

            } else if (existing instanceof PrefHandler) {
                // Already observing one, so turn it into an array.
                final List<PrefHandler> handlerList = new ArrayList<>(2);
                handlerList.add((PrefHandler) existing);
                handlerList.add(handler);
                OBSERVERS.put(pref, handlerList);

            } else {
                // Already observing multiple, so add to existing array.
                @SuppressWarnings("unchecked")
                final List<PrefHandler> handlerList = (List) existing;
                handlerList.add(handler);
            }
        }

        final String[] namesToObserve = prefsToObserve == null ? null :
                prefsToObserve.toArray(new String[prefsToObserve.size()]);

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            nativeAddObserver(prefNames, handler, namesToObserve);
        } else {
            GeckoThread.queueNativeCallUntil(
                    GeckoThread.State.PROFILE_READY, PrefsHelper.class, "nativeAddObserver",
                    String[].class, prefNames, PrefHandler.class, handler,
                    String[].class, namesToObserve);
        }
    }

    @RobocopTarget
    public synchronized static void removeObserver(final PrefHandler handler) {
        List<String> prefsToUnobserve = null;

        for (int i = OBSERVERS.size() - 1; i >= 0; i--) {
            final Object existing = OBSERVERS.valueAt(i);
            boolean removeObserver = false;

            if (existing == handler) {
                removeObserver = true;

            } else if (!(existing instanceof PrefHandler)) {
                // Removing existing handler from list.
                @SuppressWarnings("unchecked")
                final List<PrefHandler> handlerList = (List) existing;
                if (handlerList.remove(handler) && handlerList.isEmpty()) {
                    removeObserver = true;
                }
            }

            if (removeObserver) {
                // Removed last handler, so remove observer.
                if (prefsToUnobserve == null) {
                    prefsToUnobserve = new ArrayList<>();
                }
                prefsToUnobserve.add(OBSERVERS.keyAt(i));
                OBSERVERS.removeAt(i);
            }
        }

        if (prefsToUnobserve == null) {
            return;
        }

        final String[] namesToUnobserve =
                prefsToUnobserve.toArray(new String[prefsToUnobserve.size()]);

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            nativeRemoveObserver(namesToUnobserve);
        } else {
            GeckoThread.queueNativeCallUntil(
                    GeckoThread.State.PROFILE_READY, PrefsHelper.class, "nativeRemoveObserver",
                    String[].class, namesToUnobserve);
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void callPrefHandler(final PrefHandler handler, int type, final String pref,
                                        boolean boolVal, int intVal, String strVal) {

        // Some Gecko preferences use integers or strings to reference state instead of
        // directly representing the value.  Since the Java UI uses the type to determine
        // which ui elements to show and how to handle them, we need to normalize these
        // preferences to the correct type.
        if (INT_TO_STRING_PREFS.contains(pref)) {
            type = PREF_STRING;
            strVal = String.valueOf(intVal);
        } else if (INT_TO_BOOL_PREFS.contains(pref)) {
            type = PREF_BOOL;
            boolVal = intVal == 1;
        }

        switch (type) {
            case PREF_FINISH:
                handler.finish();
                return;
            case PREF_BOOL:
                handler.prefValue(pref, boolVal);
                return;
            case PREF_INT:
                handler.prefValue(pref, intVal);
                return;
            case PREF_STRING:
                handler.prefValue(pref, strVal);
                return;
        }
        throw new IllegalArgumentException();
    }

    @WrapForJNI(calledFrom = "gecko")
    private synchronized static void onPrefChange(final String pref, final int type,
                                                  final boolean boolVal, final int intVal,
                                                  final String strVal) {
        final Object existing = OBSERVERS.get(pref);

        if (existing == null) {
            return;
        }

        final Iterator<PrefHandler> itor;
        PrefHandler handler;

        if (existing instanceof PrefHandler) {
            itor = null;
            handler = (PrefHandler) existing;
        } else {
            @SuppressWarnings("unchecked")
            final List<PrefHandler> handlerList = (List) existing;
            if (handlerList.isEmpty()) {
                return;
            }
            itor = handlerList.iterator();
            handler = itor.next();
        }

        do {
            callPrefHandler(handler, type, pref, boolVal, intVal, strVal);
            handler.finish();

            handler = itor != null && itor.hasNext() ? itor.next() : null;
        } while (handler != null);
    }

    public interface PrefHandler {
        void prefValue(String pref, boolean value);
        void prefValue(String pref, int value);
        void prefValue(String pref, String value);
        void finish();
    }

    public static abstract class PrefHandlerBase implements PrefHandler {
        @Override
        public void prefValue(String pref, boolean value) {
            throw new UnsupportedOperationException(
                    "Unhandled boolean pref " + pref + "; wrong type?");
        }

        @Override
        public void prefValue(String pref, int value) {
            throw new UnsupportedOperationException(
                    "Unhandled int pref " + pref + "; wrong type?");
        }

        @Override
        public void prefValue(String pref, String value) {
            throw new UnsupportedOperationException(
                    "Unhandled String pref " + pref + "; wrong type?");
        }

        @Override
        public void finish() {
        }
    }
}
