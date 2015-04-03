/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.tests.helpers.*;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.EventCallback;
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
public class testEventDispatcher extends UITest
        implements GeckoEventListener, NativeEventListener {

    private static final String TEST_JS = "testEventDispatcher.js";
    private static final String GECKO_EVENT = "Robocop:TestGeckoEvent";
    private static final String GECKO_RESPONSE_EVENT = "Robocop:TestGeckoResponse";
    private static final String NATIVE_EVENT = "Robocop:TestNativeEvent";
    private static final String NATIVE_RESPONSE_EVENT = "Robocop:TestNativeResponse";
    private static final String NATIVE_EXCEPTION_EVENT = "Robocop:TestNativeException";

    private JavascriptBridge js;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        js = new JavascriptBridge(this);

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

        js.disconnect();
        super.tearDown();
    }

    public void testEventDispatcher() {
        GeckoHelper.blockForReady();
        NavigationHelper.enterAndLoadUrl(mStringHelper.getHarnessUrlForJavascript(TEST_JS));

        js.syncCall("send_test_message", GECKO_EVENT);
        js.syncCall("send_message_for_response", GECKO_RESPONSE_EVENT, "success");
        js.syncCall("send_message_for_response", GECKO_RESPONSE_EVENT, "error");
        js.syncCall("send_test_message", NATIVE_EVENT);
        js.syncCall("send_message_for_response", NATIVE_RESPONSE_EVENT, "success");
        js.syncCall("send_message_for_response", NATIVE_RESPONSE_EVENT, "error");
        js.syncCall("send_test_message", NATIVE_EXCEPTION_EVENT);
        js.syncCall("finish_test");
    }

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        ThreadUtils.assertOnGeckoThread();

        try {
            if (GECKO_EVENT.equals(event)) {
                checkJSONObject(message);
                checkJSONObject(message.getJSONObject("object"));

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

        } else if (NATIVE_RESPONSE_EVENT.equals(event)) {
            final String response = message.getString("response");
            if ("success".equals(response)) {
                callback.sendSuccess(response);
            } else if ("error".equals(response)) {
                callback.sendError(response);
            } else {
                fFail("Response type should be valid: " + response);
            }

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
}
