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
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.os.Bundle;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Tests the proper operation of EventDispatcher,
 * including associated NativeJSObject objects.
 */
public class testEventDispatcher extends JavascriptBridgeTest
        implements BundleEventListener, GeckoEventListener, NativeEventListener {

    private static final String TEST_JS = "testEventDispatcher.js";
    private static final String GECKO_EVENT = "Robocop:TestGeckoEvent";
    private static final String GECKO_RESPONSE_EVENT = "Robocop:TestGeckoResponse";
    private static final String NATIVE_EVENT = "Robocop:TestNativeEvent";
    private static final String NATIVE_RESPONSE_EVENT = "Robocop:TestNativeResponse";
    private static final String NATIVE_EXCEPTION_EVENT = "Robocop:TestNativeException";
    private static final String UI_EVENT = "Robocop:TestUIEvent";
    private static final String UI_RESPONSE_EVENT = "Robocop:TestUIResponse";
    private static final String BACKGROUND_EVENT = "Robocop:TestBackgroundEvent";
    private static final String BACKGROUND_RESPONSE_EVENT = "Robocop:TestBackgrondResponse";
    private static final String JS_EVENT = "Robocop:TestJSEvent";
    private static final String JS_RESPONSE_EVENT = "Robocop:TestJSResponse";

    private static final long WAIT_FOR_BUNDLE_EVENT_TIMEOUT_MILLIS = 20000; // 20 seconds

    private NativeJSObject savedMessage;

    private boolean handledGeckoEvent;
    private boolean handledNativeEvent;
    private boolean handledAsyncEvent;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        EventDispatcher.getInstance().registerGeckoThreadListener(
                (GeckoEventListener) this, GECKO_EVENT, GECKO_RESPONSE_EVENT);
        EventDispatcher.getInstance().registerGeckoThreadListener(
                (NativeEventListener) this,
                NATIVE_EVENT, NATIVE_RESPONSE_EVENT, NATIVE_EXCEPTION_EVENT);
    }

    @Override
    public void tearDown() throws Exception {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(
                (GeckoEventListener) this, GECKO_EVENT, GECKO_RESPONSE_EVENT);
        EventDispatcher.getInstance().unregisterGeckoThreadListener(
                (NativeEventListener) this,
                NATIVE_EVENT, NATIVE_RESPONSE_EVENT, NATIVE_EXCEPTION_EVENT);

        super.tearDown();
    }

    private synchronized void waitForAsyncEvent() {
        final long startTime = System.nanoTime();
        while (!handledAsyncEvent) {
            if (System.nanoTime() - startTime
                    >= WAIT_FOR_BUNDLE_EVENT_TIMEOUT_MILLIS * 1e6 /* ns per ms */) {
                fFail("Should have completed event before timeout");
            }
            try {
                wait(1000); // Wait for 1 second at a time.
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

        getJS().syncCall("send_test_message", GECKO_EVENT);
        fAssertTrue("Should have handled Gecko event synchronously", handledGeckoEvent);

        getJS().syncCall("send_message_for_response", GECKO_RESPONSE_EVENT, "success");
        getJS().syncCall("send_message_for_response", GECKO_RESPONSE_EVENT, "error");

        getJS().syncCall("send_test_message", NATIVE_EVENT);
        fAssertTrue("Should have handled native event synchronously", handledNativeEvent);

        getJS().syncCall("send_message_for_response", NATIVE_RESPONSE_EVENT, "success");
        getJS().syncCall("send_message_for_response", NATIVE_RESPONSE_EVENT, "error");

        getJS().syncCall("send_test_message", NATIVE_EXCEPTION_EVENT);

        // Test global EventDispatcher.
        testScope("global");

        // Test GeckoView-specific EventDispatcher.
        testScope("window");

        getJS().syncCall("finish_test");
    }

    private static EventDispatcher getDispatcher(final String scope) {
        if ("global".equals(scope)) {
            return EventDispatcher.getInstance();
        }
        if ("window".equals(scope)) {
            return GeckoApp.getEventDispatcher();
        }
        fFail("scope argument should be valid string");
        return null;
    }

    private void testScope(final String scope) {
        // Test UI thread events.
        getDispatcher(scope).registerUiThreadListener(
                this, UI_EVENT, UI_RESPONSE_EVENT);

        testThreadEvents(scope, UI_EVENT, UI_RESPONSE_EVENT);

        getDispatcher(scope).unregisterUiThreadListener(
                this, UI_EVENT, UI_RESPONSE_EVENT);

        // Test background thread events.
        getDispatcher(scope).registerBackgroundThreadListener(
                this, BACKGROUND_EVENT, BACKGROUND_RESPONSE_EVENT);

        testThreadEvents(scope, BACKGROUND_EVENT, BACKGROUND_RESPONSE_EVENT);

        getDispatcher(scope).unregisterBackgroundThreadListener(
                this, BACKGROUND_EVENT, BACKGROUND_RESPONSE_EVENT);

        // Test Gecko thread events in JS.
        getJS().syncCall("register_js_events", scope, JS_EVENT, JS_RESPONSE_EVENT);

        getJS().syncCall("dispatch_test_message", scope, JS_EVENT);
        getJS().syncCall("dispatch_message_for_response", scope, JS_RESPONSE_EVENT, "success");
        getJS().syncCall("dispatch_message_for_response", scope, JS_RESPONSE_EVENT, "error");

        dispatchMessage(scope, JS_EVENT);
        dispatchMessageForResponse(scope, JS_RESPONSE_EVENT, "success");
        dispatchMessageForResponse(scope, JS_RESPONSE_EVENT, "error");

        getJS().syncCall("unregister_js_events", scope, JS_EVENT, JS_RESPONSE_EVENT);
    }

    private void testThreadEvents(final String scope, final String event, final String responseEvent) {
        getJS().syncCall("send_test_message", event);
        waitForAsyncEvent();

        getJS().syncCall("send_message_for_response", responseEvent, "success");
        waitForAsyncEvent();

        getJS().syncCall("send_message_for_response", responseEvent, "error");
        waitForAsyncEvent();

        getJS().syncCall("dispatch_test_message", scope, event);
        waitForAsyncEvent();

        getJS().syncCall("dispatch_message_for_response", scope, responseEvent, "success");
        waitForAsyncEvent();

        getJS().syncCall("dispatch_message_for_response", scope, responseEvent, "error");
        waitForAsyncEvent();

        dispatchMessage(scope, event);
        waitForAsyncEvent();

        dispatchMessageForResponse(scope, responseEvent, "success");
        waitForAsyncEvent();

        dispatchMessageForResponse(scope, responseEvent, "error");
        waitForAsyncEvent();
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {

        if (UI_EVENT.equals(event) || UI_RESPONSE_EVENT.equals(event)) {
            fAssertTrue("UI event should be on UI thread", ThreadUtils.isOnUiThread());

        } else if (BACKGROUND_EVENT.equals(event) || BACKGROUND_RESPONSE_EVENT.equals(event)) {
            fAssertTrue("Background event should be on background thread",
                        ThreadUtils.isOnBackgroundThread());

        } else {
            fFail("Event type should be valid: " + event);
        }

        if (UI_EVENT.equals(event) || BACKGROUND_EVENT.equals(event)) {
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

        } else if (UI_RESPONSE_EVENT.equals(event) || BACKGROUND_RESPONSE_EVENT.equals(event)) {
            final String response = message.getString("response");
            if ("success".equals(response)) {
                callback.sendSuccess(response);
            } else if ("error".equals(response)) {
                callback.sendError(response);
            } else {
                fFail("Response type should be valid: " + response);
            }

        } else {
            fFail("Event type should be valid: " + event);
        }

        notifyAsyncEvent();
    }

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        ThreadUtils.assertOnGeckoThread();

        try {
            if (GECKO_EVENT.equals(event)) {
                checkJSONObject(message);
                checkJSONObject(message.getJSONObject("object"));
                handledGeckoEvent = true;

            } else if (GECKO_RESPONSE_EVENT.equals(event)) {
                final String response = message.getString("response");
                if ("success".equals(response)) {
                    EventDispatcher.sendResponse(message, response);
                } else if ("error".equals(response)) {
                    EventDispatcher.sendError(message, response);
                } else {
                    fFail("Response type should be valid: " + response);
                }

            } else {
                fFail("Event type should be valid: " + event);
            }
        } catch (final JSONException e) {
            fFail(e.toString());
        }
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        ThreadUtils.assertOnGeckoThread();

        if (NATIVE_EVENT.equals(event)) {
            checkNativeJSObject(message);
            checkNativeJSObject(message.getObject("object"));
            fAssertNotSame("optObject returns existent value",
                    null, message.optObject("object", null));
            fAssertSame("optObject returns fallback value if nonexistent",
                    null, message.optObject("nonexistent_object", null));

            final NativeJSObject[] objectArray = message.getObjectArray("objectArray");
            fAssertNotNull("Native object array should exist", objectArray);
            fAssertEquals("Native object array has correct length", 2, objectArray.length);
            fAssertSame("Native object array index 0 has correct value", null, objectArray[0]);
            fAssertNotSame("Native object array index 1 has correct value", null, objectArray[1]);
            checkNativeJSObject(objectArray[1]);
            fAssertNotSame("optObjectArray returns existent value",
                null, message.optObjectArray("objectArray", null));
            fAssertSame("optObjectArray returns fallback value if nonexistent",
                null, message.optObjectArray("nonexistent_objectArray", null));

            final Bundle bundle = message.toBundle();
            checkBundle(bundle);
            checkBundle(bundle.getBundle("object"));
            fAssertNotSame("optBundle returns property value if it exists",
                    null, message.optBundle("object", null));
            fAssertSame("optBundle returns fallback value if property does not exist",
                    null, message.optBundle("nonexistent_object", null));

            final Bundle[] bundleArray = message.getBundleArray("objectArray");
            fAssertNotNull("Native bundle array should exist", bundleArray);
            fAssertEquals("Native bundle array has correct length", 2, bundleArray.length);
            fAssertSame("Native bundle array index 0 has correct value", null, bundleArray[0]);
            fAssertNotSame("Native bundle array index 1 has correct value", null, bundleArray[1]);
            checkBundle(bundleArray[1]);
            fAssertNotSame("optBundleArray returns existent value",
                null, message.optBundleArray("objectArray", null));
            fAssertSame("optBundleArray returns fallback value if nonexistent",
                null, message.optBundleArray("nonexistent_objectArray", null));

            handledNativeEvent = true;

        } else if (NATIVE_RESPONSE_EVENT.equals(event)) {
            final String response = message.getString("response");
            if ("success".equals(response)) {
                callback.sendSuccess(response);
            } else if ("error".equals(response)) {
                callback.sendError(response);
            } else {
                fFail("Response type should be valid: " + response);
            }

            // Save this message for post-disposal check.
            savedMessage = message;

        } else if (NATIVE_EXCEPTION_EVENT.equals(event)) {
            // Make sure we throw the right exceptions.
            try {
                message.getString(null);
                fFail("null property name should throw IllegalArgumentException");
            } catch (final IllegalArgumentException e) {
            }

            try {
                message.getString("nonexistent_string");
                fFail("Nonexistent property name should throw InvalidPropertyException");
            } catch (final NativeJSObject.InvalidPropertyException e) {
            }

            try {
                message.getString("int");
                fFail("Wrong property type should throw InvalidPropertyException");
            } catch (final NativeJSObject.InvalidPropertyException e) {
            }

            fAssertNotSame("Should have saved a message", null, savedMessage);
            try {
                savedMessage.toString();
                fFail("Using NativeJSContainer should throw after disposal");
            } catch (final NullPointerException e) {
            }

            // Save this test for last; make sure EventDispatcher catches InvalidPropertyException.
            message.getString("nonexistent_string");
            fFail("EventDispatcher should catch InvalidPropertyException");

        } else {
            fFail("Event type should be valid: " + event);
        }
    }

    private void checkBundle(final Bundle bundle) {
        fAssertEquals("Bundle boolean has correct value", true, bundle.getBoolean("boolean"));
        fAssertEquals("Bundle int has correct value", 1, bundle.getInt("int"));
        fAssertEquals("Bundle double has correct value", 0.5, bundle.getDouble("double"));
        fAssertEquals("Bundle string has correct value", "foo", bundle.getString("string"));

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

        // XXX add mixedArray check when we remove NativeJSObject
    }

    private void checkJSONObject(final JSONObject object) throws JSONException {
        fAssertEquals("JSON boolean has correct value", true, object.getBoolean("boolean"));
        fAssertEquals("JSON int has correct value", 1, object.getInt("int"));
        fAssertEquals("JSON double has correct value", 0.5, object.getDouble("double"));
        fAssertEquals("JSON string has correct value", "foo", object.getString("string"));

        final JSONArray booleanArray = object.getJSONArray("booleanArray");
        fAssertNotNull("JSON boolean array should exist", booleanArray);
        fAssertEquals("JSON boolean array has correct length", 2, booleanArray.length());
        fAssertEquals("JSON boolean array index 0 has correct value",
            false, booleanArray.getBoolean(0));
        fAssertEquals("JSON boolean array index 1 has correct value",
            true, booleanArray.getBoolean(1));

        final JSONArray intArray = object.getJSONArray("intArray");
        fAssertNotNull("JSON int array should exist", intArray);
        fAssertEquals("JSON int array has correct length", 2, intArray.length());
        fAssertEquals("JSON int array index 0 has correct value",
            2, intArray.getInt(0));
        fAssertEquals("JSON int array index 1 has correct value",
            3, intArray.getInt(1));

        final JSONArray doubleArray = object.getJSONArray("doubleArray");
        fAssertNotNull("JSON double array should exist", doubleArray);
        fAssertEquals("JSON double array has correct length", 2, doubleArray.length());
        fAssertEquals("JSON double array index 0 has correct value",
            1.5, doubleArray.getDouble(0));
        fAssertEquals("JSON double array index 1 has correct value",
            2.5, doubleArray.getDouble(1));

        final JSONArray stringArray = object.getJSONArray("stringArray");
        fAssertNotNull("JSON string array should exist", stringArray);
        fAssertEquals("JSON string array has correct length", 2, stringArray.length());
        fAssertEquals("JSON string array index 0 has correct value",
            "bar", stringArray.getString(0));
        fAssertEquals("JSON string array index 1 has correct value",
            "baz", stringArray.getString(1));
    }

    private void checkNativeJSObject(final NativeJSObject object) {
        fAssertEquals("Native boolean has correct value",
                true, object.getBoolean("boolean"));
        fAssertEquals("optBoolean returns existent value",
                true, object.optBoolean("boolean", false));
        fAssertEquals("optBoolean returns fallback value if nonexistent",
                false, object.optBoolean("nonexistent_boolean", false));

        fAssertEquals("Native int has correct value",
                1, object.getInt("int"));
        fAssertEquals("optInt returns existent value",
                1, object.optInt("int", 0));
        fAssertEquals("optInt returns fallback value if nonexistent",
                0, object.optInt("nonexistent_int", 0));

        fAssertEquals("Native double has correct value",
                0.5, object.getDouble("double"));
        fAssertEquals("optDouble returns existent value",
                0.5, object.optDouble("double", -0.5));
        fAssertEquals("optDouble returns fallback value if nonexistent",
                -0.5, object.optDouble("nonexistent_double", -0.5));

        fAssertEquals("Native string has correct value",
                "foo", object.getString("string"));
        fAssertEquals("optDouble returns existent value",
                "foo", object.optString("string", "bar"));
        fAssertEquals("optDouble returns fallback value if nonexistent",
                "bar", object.optString("nonexistent_string", "bar"));

        final boolean[] booleanArray = object.getBooleanArray("booleanArray");
        fAssertNotNull("Native boolean array should exist", booleanArray);
        fAssertEquals("Native boolean array has correct length", 2, booleanArray.length);
        fAssertEquals("Native boolean array index 0 has correct value", false, booleanArray[0]);
        fAssertEquals("Native boolean array index 1 has correct value", true, booleanArray[1]);
        fAssertNotSame("optBooleanArray returns existent value",
            null, object.optBooleanArray("booleanArray", null));
        fAssertSame("optBooleanArray returns fallback value if nonexistent",
            null, object.optBooleanArray("nonexistent_booleanArray", null));

        final int[] intArray = object.getIntArray("intArray");
        fAssertNotNull("Native int array should exist", intArray);
        fAssertEquals("Native int array has correct length", 2, intArray.length);
        fAssertEquals("Native int array index 0 has correct value", 2, intArray[0]);
        fAssertEquals("Native int array index 1 has correct value", 3, intArray[1]);
        fAssertNotSame("optIntArray returns existent value",
            null, object.optIntArray("intArray", null));
        fAssertSame("optIntArray returns fallback value if nonexistent",
            null, object.optIntArray("nonexistent_intArray", null));

        final double[] doubleArray = object.getDoubleArray("doubleArray");
        fAssertNotNull("Native double array should exist", doubleArray);
        fAssertEquals("Native double array has correct length", 2, doubleArray.length);
        fAssertEquals("Native double array index 0 has correct value", 1.5, doubleArray[0]);
        fAssertEquals("Native double array index 1 has correct value", 2.5, doubleArray[1]);
        fAssertNotSame("optDoubleArray returns existent value",
            null, object.optDoubleArray("doubleArray", null));
        fAssertSame("optDoubleArray returns fallback value if nonexistent",
            null, object.optDoubleArray("nonexistent_doubleArray", null));

        final String[] stringArray = object.getStringArray("stringArray");
        fAssertNotNull("Native string array should exist", stringArray);
        fAssertEquals("Native string array has correct length", 2, stringArray.length);
        fAssertEquals("Native string array index 0 has correct value", "bar", stringArray[0]);
        fAssertEquals("Native string array index 1 has correct value", "baz", stringArray[1]);
        fAssertNotSame("optStringArray returns existent value",
            null, object.optStringArray("stringArray", null));
        fAssertSame("optStringArray returns fallback value if nonexistent",
            null, object.optStringArray("nonexistent_stringArray", null));

        fAssertEquals("Native has(null) is false", false, object.has("null"));
        fAssertEquals("Native has(emptyString) is true", true, object.has("emptyString"));

        fAssertEquals("Native optBoolean returns fallback value if null",
            true, object.optBoolean("null", true));
        fAssertEquals("Native optInt returns fallback value if null",
            42, object.optInt("null", 42));
        fAssertEquals("Native optDouble returns fallback value if null",
            -3.1415926535, object.optDouble("null", -3.1415926535));
        fAssertEquals("Native optString returns fallback value if null",
            "baz", object.optString("null", "baz"));

        fAssertNotEquals("Native optString does not return fallback value if emptyString",
            "baz", object.optString("emptyString", "baz"));
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

    public void dispatchMessageForResponse(final String scope, final String type,
                                           final String response) {
        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("response", response);

        getDispatcher(scope).dispatch(type, bundle, new EventCallback() {
            @Override
            public void sendSuccess(final Object result) {
                // If the original request was on the UI thread, the response would happen
                // on the UI thread as well. Otherwise, the response happens on the
                // background thread. In this case, because the request was on the testing
                // thread, the response thread defaults to the background thread.
                fAssertTrue("JS success response should be on background thread",
                            ThreadUtils.isOnBackgroundThread());
                fAssertEquals("JS success response is correct", response, result);
            }

            @Override
            public void sendError(final Object error) {
                fAssertTrue("JS error response should be on background thread",
                            ThreadUtils.isOnBackgroundThread());
                fAssertEquals("JS error response is correct", response, error);
            }
        });
    }
}
