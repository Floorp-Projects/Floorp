/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * Tests the proper operation of EventDispatcher,
 */
public class testEventDispatcher extends JavascriptBridgeTest implements BundleEventListener {

    private static final String TEST_JS = "testEventDispatcher.js";
    private static final String GECKO_EVENT = "Robocop:TestGeckoEvent";
    private static final String GECKO_RESPONSE_EVENT = "Robocop:TestGeckoResponse";
    private static final String UI_EVENT = "Robocop:TestUIEvent";
    private static final String UI_RESPONSE_EVENT = "Robocop:TestUIResponse";
    private static final String BACKGROUND_EVENT = "Robocop:TestBackgroundEvent";
    private static final String BACKGROUND_RESPONSE_EVENT = "Robocop:TestBackgrondResponse";
    private static final String JS_EVENT = "Robocop:TestJSEvent";
    private static final String JS_RESPONSE_EVENT = "Robocop:TestJSResponse";

    private static final long WAIT_FOR_BUNDLE_EVENT_TIMEOUT_MILLIS = 20000; // 20 seconds

    private boolean handledAsyncEvent;

    private synchronized void waitForAsyncEvent() {
        final long startTime = System.nanoTime();
        while (!handledAsyncEvent) {
            if (System.nanoTime() - startTime
                    >= WAIT_FOR_BUNDLE_EVENT_TIMEOUT_MILLIS * 1e6 /* ns per ms */) {
                fFail("Should have completed event before timeout");
            }
            try {
                wait(100); // Wait for 100ms at a time.
            } catch (final InterruptedException e) {
                // Attempt waiting again.
            }
        }
        handledAsyncEvent = false;
    }

    private synchronized void notifyAsyncEvent() {
        handledAsyncEvent = true;
        notifyAll();
    }

    public void testEventDispatcher() {
        blockForReadyAndLoadJS(TEST_JS);

        // Test global EventDispatcher.
        testScope("global");

        // Test GeckoView-specific EventDispatcher.
        testScope("window");

        getJS().syncCall("finish_test");
    }

    private EventDispatcher getDispatcher(final String scope) {
        if ("global".equals(scope)) {
            return EventDispatcher.getInstance();
        }
        if ("window".equals(scope)) {
            return ((GeckoApp) getActivity()).getAppEventDispatcher();
        }
        fFail("scope argument should be valid string");
        return null;
    }

    private void testScope(final String scope) {
        // Test Gecko thread events.
        getDispatcher(scope).registerGeckoThreadListener(
                this, GECKO_EVENT, GECKO_RESPONSE_EVENT);

        testThreadEvents(scope, GECKO_EVENT, GECKO_RESPONSE_EVENT, /* wait */ true);

        getDispatcher(scope).unregisterGeckoThreadListener(
                this, GECKO_EVENT, GECKO_RESPONSE_EVENT);

        // Test UI thread events.
        getDispatcher(scope).registerUiThreadListener(
                this, UI_EVENT, UI_RESPONSE_EVENT);

        testThreadEvents(scope, UI_EVENT, UI_RESPONSE_EVENT, /* wait */ true);

        getDispatcher(scope).unregisterUiThreadListener(
                this, UI_EVENT, UI_RESPONSE_EVENT);

        // Test background thread events.
        getDispatcher(scope).registerBackgroundThreadListener(
                this, BACKGROUND_EVENT, BACKGROUND_RESPONSE_EVENT);

        testThreadEvents(scope, BACKGROUND_EVENT, BACKGROUND_RESPONSE_EVENT, /* wait */ true);

        getDispatcher(scope).unregisterBackgroundThreadListener(
                this, BACKGROUND_EVENT, BACKGROUND_RESPONSE_EVENT);

        // Test Gecko thread events in JS.
        getJS().syncCall("register_js_events", scope, JS_EVENT, JS_RESPONSE_EVENT);

        final int count = testThreadEvents(
                scope, JS_EVENT, JS_RESPONSE_EVENT, /* wait */ false);

        getJS().syncCall("unregister_js_events", scope, JS_EVENT, JS_RESPONSE_EVENT, count);
    }

    private int testThreadResponseEvents(final String method, final String scope,
                                          final String responseEvent, final String mode,
                                          final boolean wait) {
        int count = 0;
        // Try all the values in the reference bundle as callback values.
        for (final String key : createBundle().keys()) {
            if (key.indexOf("Array") >= 0) {
                // Skip arrays for now because they're complicated to compare.
                continue;
            }

            getJS().syncCall(method, scope, responseEvent, mode, key);
            count++;

            if (wait) {
                waitForAsyncEvent();
            }
        }
        return count;
    }

    private int testThreadEvents(final String scope, final String event,
                                 final String responseEvent, final boolean wait) {
        int count = 0;

        getJS().syncCall("send_test_message", scope, event);
        count++;

        if (wait) {
            waitForAsyncEvent();
        }

        count += testThreadResponseEvents("send_message_for_response",
                scope, responseEvent, "success", wait);
        count += testThreadResponseEvents("send_message_for_response",
                scope, responseEvent, "error", wait);

        getJS().syncCall("dispatch_test_message", scope, event);
        count++;

        if (wait) {
            waitForAsyncEvent();
        }

        count += testThreadResponseEvents("dispatch_message_for_response",
                scope, responseEvent, "success", wait);

        count += testThreadResponseEvents("dispatch_message_for_response",
                scope, responseEvent, "error", wait);

        dispatchMessage(scope, event);
        count++;

        if (wait) {
            waitForAsyncEvent();
        }

        count += dispatchMessagesForResponse(scope, responseEvent, "success", wait);
        count += dispatchMessagesForResponse(scope, responseEvent, "error", wait);

        return count;
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {

        if (GECKO_EVENT.equals(event) || GECKO_RESPONSE_EVENT.equals(event)) {
            fAssertTrue("Gecko event should be on Gecko thread", ThreadUtils.isOnGeckoThread());

        } else if (UI_EVENT.equals(event) || UI_RESPONSE_EVENT.equals(event)) {
            fAssertTrue("UI event should be on UI thread", ThreadUtils.isOnUiThread());

        } else if (BACKGROUND_EVENT.equals(event) || BACKGROUND_RESPONSE_EVENT.equals(event)) {
            fAssertTrue("Background event should be on background thread",
                        ThreadUtils.isOnBackgroundThread());

        } else {
            fFail("Event type should be valid: " + event);
        }

        if (GECKO_EVENT.equals(event) || UI_EVENT.equals(event) || BACKGROUND_EVENT.equals(event)) {
            checkBundle(message);
            checkBundle(message.getBundle("object"));
            fAssertSame("Bundle null object has correct value", null, message.getBundle("nullObject"));
            fAssertSame("Bundle nonexistent object returns null",
                    null, message.getBundle("nonexistentObject"));

            final GeckoBundle[] objectArray = message.getBundleArray("objectArray");
            fAssertNotNull("Bundle object array should exist", objectArray);
            fAssertEquals("Bundle object array has correct length", 2, objectArray.length);
            fAssertSame("Bundle object array index 0 has correct value", null, objectArray[0]);
            checkBundle(objectArray[1]);

            final GeckoBundle[] objectArrayOfNull = message.getBundleArray("objectArrayOfNull");
            fAssertNotNull("Bundle object array of null should exist", objectArrayOfNull);
            fAssertEquals("Bundle object array of null has correct length", 2, objectArrayOfNull.length);
            fAssertSame("Bundle object array of null index 0 has correct value",
                    null, objectArrayOfNull[0]);
            fAssertSame("Bundle object array of null index 1 has correct value",
                    null, objectArrayOfNull[1]);

            final GeckoBundle[] emptyObjectArray = message.getBundleArray("emptyObjectArray");
            fAssertNotNull("Bundle empty object array should exist", emptyObjectArray);
            fAssertEquals("Bundle empty object array has correct length", 0, emptyObjectArray.length);

            fAssertSame("Bundle null object array exists",
                    null, message.getBundleArray("nullObjectArray"));

            fAssertSame("Bundle nonexistent object array returns null",
                    null, message.getBundleArray("nonexistentObjectArray"));

        } else if (GECKO_RESPONSE_EVENT.equals(event) || UI_RESPONSE_EVENT.equals(event) ||
                BACKGROUND_RESPONSE_EVENT.equals(event)) {
            final String mode = message.getString("mode");
            final String key = message.getString("key");
            final Object response = getTypedResponse(key);

            if ("success".equals(mode)) {
                callback.sendSuccess(response);
            } else if ("error".equals(mode)) {
                callback.sendError(response);
            } else {
                fFail("Response type should be valid: " + response);
            }

        } else {
            fFail("Event type should be valid: " + event);
        }

        notifyAsyncEvent();
    }

    private Object getTypedResponse(final String key) {
        final Object response = createBundle().get(key);
        if ("byte".equals(key)) {
            return ((Number) response).byteValue();
        } else if ("short".equals(key)) {
            return ((Number) response).shortValue();
        } else if ("float".equals(key)) {
            return ((Number) response).floatValue();
        } else if ("long".equals(key)) {
            return ((Number) response).longValue();
        } else if ("char".equals(key)) {
            return ((String) response).charAt(0);
        }
        return response;
    }

    private void checkBundle(final GeckoBundle bundle) {
        fAssertEquals("Bundle boolean has correct value", true, bundle.getBoolean("boolean"));
        fAssertEquals("Bundle int has correct value", 1, bundle.getInt("int"));
        fAssertEquals("Bundle double has correct value", 0.5, bundle.getDouble("double"));
        fAssertEquals("Bundle string has correct value", "foo", bundle.getString("string"));

        fAssertEquals("Bundle default boolean has correct value",
                true, bundle.getBoolean("nonexistentBoolean", true));
        fAssertEquals("Bundle default int has correct value",
                1, bundle.getInt("nonexistentInt", 1));
        fAssertEquals("Bundle default double has correct value",
                0.5, bundle.getDouble("nonexistentDouble", 0.5));
        fAssertEquals("Bundle default string has correct value",
                "foo", bundle.getString("nonexistentString", "foo"));

        fAssertEquals("Bundle nonexistent boolean returns false",
                false, bundle.getBoolean("nonexistentBoolean"));
        fAssertEquals("Bundle nonexistent int returns 0",
                0, bundle.getInt("nonexistentInt"));
        fAssertEquals("Bundle nonexistent double returns 0.0",
                0.0, bundle.getDouble("nonexistentDouble"));
        fAssertSame("Bundle nonexistent string returns null",
                null, bundle.getString("nonexistentString"));

        fAssertSame("Bundle null string has correct value", null, bundle.getString("nullString"));
        fAssertEquals("Bundle empty string has correct value", "", bundle.getString("emptyString"));
        fAssertEquals("Bundle default null string is correct", "foo",
                      bundle.getString("nullString", "foo"));
        fAssertEquals("Bundle default empty string is correct", "",
                      bundle.getString("emptyString", "foo"));

        final boolean[] booleanArray = bundle.getBooleanArray("booleanArray");
        fAssertNotNull("Bundle boolean array should exist", booleanArray);
        fAssertEquals("Bundle boolean array has correct length", 2, booleanArray.length);
        fAssertEquals("Bundle boolean array index 0 has correct value", false, booleanArray[0]);
        fAssertEquals("Bundle boolean array index 1 has correct value", true, booleanArray[1]);

        final int[] intArray = bundle.getIntArray("intArray");
        fAssertNotNull("Bundle int array should exist", intArray);
        fAssertEquals("Bundle int array has correct length", 2, intArray.length);
        fAssertEquals("Bundle int array index 0 has correct value", 2, intArray[0]);
        fAssertEquals("Bundle int array index 1 has correct value", 3, intArray[1]);

        final double[] doubleArray = bundle.getDoubleArray("doubleArray");
        fAssertNotNull("Bundle double array should exist", doubleArray);
        fAssertEquals("Bundle double array has correct length", 2, doubleArray.length);
        fAssertEquals("Bundle double array index 0 has correct value", 1.5, doubleArray[0]);
        fAssertEquals("Bundle double array index 1 has correct value", 2.5, doubleArray[1]);

        final String[] stringArray = bundle.getStringArray("stringArray");
        fAssertNotNull("Bundle string array should exist", stringArray);
        fAssertEquals("Bundle string array has correct length", 2, stringArray.length);
        fAssertEquals("Bundle string array index 0 has correct value", "bar", stringArray[0]);
        fAssertEquals("Bundle string array index 1 has correct value", "baz", stringArray[1]);

        final String[] stringArrayOfNull = bundle.getStringArray("stringArrayOfNull");
        fAssertNotNull("Bundle string array of null should exist", stringArrayOfNull);
        fAssertEquals("Bundle string array of null has correct length", 2, stringArrayOfNull.length);
        fAssertSame("Bundle string array of null index 0 has correct value",
                null, stringArrayOfNull[0]);
        fAssertSame("Bundle string array of null index 1 has correct value",
                null, stringArrayOfNull[1]);

        final boolean[] emptyBooleanArray = bundle.getBooleanArray("emptyBooleanArray");
        fAssertNotNull("Bundle empty boolean array should exist", emptyBooleanArray);
        fAssertEquals("Bundle empty boolean array has correct length", 0, emptyBooleanArray.length);

        final int[] emptyIntArray = bundle.getIntArray("emptyIntArray");
        fAssertNotNull("Bundle empty int array should exist", emptyIntArray);
        fAssertEquals("Bundle empty int array has correct length", 0, emptyIntArray.length);

        final double[] emptyDoubleArray = bundle.getDoubleArray("emptyDoubleArray");
        fAssertNotNull("Bundle empty double array should exist", emptyDoubleArray);
        fAssertEquals("Bundle empty double array has correct length", 0, emptyDoubleArray.length);

        final String[] emptyStringArray = bundle.getStringArray("emptyStringArray");
        fAssertNotNull("Bundle empty String array should exist", emptyStringArray);
        fAssertEquals("Bundle empty String array has correct length", 0, emptyStringArray.length);

        fAssertSame("Bundle null boolean array exists",
                null, bundle.getBooleanArray("nullBooleanArray"));
        fAssertSame("Bundle null int array exists",
                null, bundle.getIntArray("nullIntArray"));
        fAssertSame("Bundle null double array exists",
                null, bundle.getDoubleArray("nullDoubleArray"));
        fAssertSame("Bundle null string array exists",
                null, bundle.getStringArray("nullStringArray"));

        fAssertSame("Bundle nonexistent boolean array returns null",
                null, bundle.getBooleanArray("nonexistentBooleanArray"));
        fAssertSame("Bundle nonexistent int array returns null",
                null, bundle.getIntArray("nonexistentIntArray"));
        fAssertSame("Bundle nonexistent double array returns null",
                null, bundle.getDoubleArray("nonexistentDoubleArray"));
        fAssertSame("Bundle nonexistent string array returns null",
                null, bundle.getStringArray("nonexistentStringArray"));

        final int[] mixedIntArray = bundle.getIntArray("mixedArray");
        fAssertNotNull("Bundle mixed int array should exist", mixedIntArray);
        fAssertEquals("Bundle mixed int array has correct length", 2, mixedIntArray.length);
        fAssertEquals("Bundle mixed int array index 0 has correct value", 1, mixedIntArray[0]);
        fAssertEquals("Bundle mixed int array index 1 has correct value", 1, mixedIntArray[1]);

        final double[] mixedDoubleArray = bundle.getDoubleArray("mixedArray");
        fAssertNotNull("Bundle mixed double array should exist", mixedDoubleArray);
        fAssertEquals("Bundle mixed double array has correct length", 2, mixedDoubleArray.length);
        fAssertEquals("Bundle mixed double array index 0 has correct value", 1.0, mixedDoubleArray[0]);
        fAssertEquals("Bundle mixed double array index 1 has correct value", 1.5, mixedDoubleArray[1]);
    }

    private static GeckoBundle createInnerBundle() {
        final GeckoBundle bundle = new GeckoBundle();

        bundle.putBoolean("boolean", true);
        bundle.putBooleanArray("booleanArray", new boolean[] {false, true});

        bundle.putInt("int", 1);
        bundle.putIntArray("intArray", new int[] {2, 3});

        bundle.putDouble("double", 0.5);
        bundle.putDoubleArray("doubleArray", new double[] {1.5, 2.5});

        bundle.putString("string", "foo");
        bundle.putString("nullString", null);
        bundle.putString("emptyString", "");
        bundle.putStringArray("stringArray", new String[] {"bar", "baz"});
        bundle.putStringArray("stringArrayOfNull", new String[2]);

        bundle.putBooleanArray("emptyBooleanArray", new boolean[0]);
        bundle.putIntArray("emptyIntArray", new int[0]);
        bundle.putDoubleArray("emptyDoubleArray", new double[0]);
        bundle.putStringArray("emptyStringArray", new String[0]);

        bundle.putBooleanArray("nullBooleanArray", (boolean[]) null);
        bundle.putIntArray("nullIntArray", (int[]) null);
        bundle.putDoubleArray("nullDoubleArray", (double[]) null);
        bundle.putStringArray("nullStringArray", (String[]) null);

        bundle.putDoubleArray("mixedArray", new double[] {1.0, 1.5});

        bundle.putInt("byte", 1);
        bundle.putInt("short", 1);
        bundle.putDouble("float", 0.5);
        bundle.putDouble("long", 1.0);
        bundle.putString("char", "f");

        return bundle;
    }

    private static GeckoBundle createBundle() {
        final GeckoBundle outer = createInnerBundle();
        final GeckoBundle inner = createInnerBundle();

        outer.putBundle("object", inner);
        outer.putBundle("nullObject", null);
        outer.putBundleArray("objectArray", new GeckoBundle[] {null, inner});
        outer.putBundleArray("objectArrayOfNull", new GeckoBundle[2]);
        outer.putBundleArray("emptyObjectArray", new GeckoBundle[0]);
        outer.putBundleArray("nullObjectArray", (GeckoBundle[]) null);

        return outer;
    }

    public void dispatchMessage(final String scope, final String type) {
        getDispatcher(scope).dispatch(type, createBundle());
    }

    private int dispatchMessagesForResponse(final String scope, final String responseEvent,
                                            final String mode, final boolean wait) {
        final GeckoBundle refBundle = createBundle();
        int count = 0;
        // Try all the values in the reference bundle as callback values.
        for (final String key : refBundle.keys()) {
            if (key.indexOf("Array") >= 0 || refBundle.get(key) instanceof Number) {
                // We don't support numbers and arrays as callback results.
                // Skip arrays for now because they're complicated to compare.
                continue;
            }

            dispatchMessageForResponse(scope, responseEvent, "success", key, refBundle);
            count++;

            if (wait) {
                waitForAsyncEvent();
            }
        }
        return count;
    }

    public void dispatchMessageForResponse(final String scope, final String type,
                                           final String mode, final String key,
                                           final GeckoBundle refBundle) {
        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("mode", mode);
        bundle.putString("key", key);

        getDispatcher(scope).dispatch(type, bundle, new EventCallback() {
            @Override
            public void sendSuccess(final Object result) {
                // If the original request was on the UI thread, the response would happen
                // on the UI thread as well. Otherwise, the response happens on the
                // background thread. In this case, because the request was on the testing
                // thread, the response thread defaults to the background thread.
                fAssertTrue("JS success response should be on background thread",
                            ThreadUtils.isOnBackgroundThread());
                fAssertEquals("JS response mode is correct", mode, "success");
                fAssertEquals("JS success response is correct", refBundle.get(key), result);
            }

            @Override
            public void sendError(final Object error) {
                fAssertTrue("JS error response should be on background thread",
                            ThreadUtils.isOnBackgroundThread());
                fAssertEquals("JS response mode is correct", mode, "error");
                fAssertEquals("JS error response is correct", refBundle.get(key), error);
            }
        });
    }
}
