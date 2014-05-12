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

    private JavascriptBridge js;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        js = new JavascriptBridge(this);

        EventDispatcher.getInstance().registerGeckoThreadListener(
                (GeckoEventListener) this, GECKO_EVENT, GECKO_RESPONSE_EVENT);
        EventDispatcher.getInstance().registerGeckoThreadListener(
                (NativeEventListener) this, NATIVE_EVENT, NATIVE_RESPONSE_EVENT);
    }

    @Override
    public void tearDown() throws Exception {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(
                (GeckoEventListener) this, GECKO_EVENT, GECKO_RESPONSE_EVENT);
        EventDispatcher.getInstance().unregisterGeckoThreadListener(
                (NativeEventListener) this, NATIVE_EVENT, NATIVE_RESPONSE_EVENT);

        js.disconnect();
        super.tearDown();
    }

    public void testEventDispatcher() {
        GeckoHelper.blockForReady();
        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_JS_HARNESS_URL +
                                         "?path=" + TEST_JS);

        js.syncCall("send_test_message", GECKO_EVENT);
        js.syncCall("send_message_for_response", GECKO_RESPONSE_EVENT, "success");
        js.syncCall("send_message_for_response", GECKO_RESPONSE_EVENT, "error");
        js.syncCall("send_test_message", NATIVE_EVENT);
        js.syncCall("send_message_for_response", NATIVE_RESPONSE_EVENT, "success");
        js.syncCall("send_message_for_response", NATIVE_RESPONSE_EVENT, "error");
        js.syncCall("send_message_for_response", NATIVE_RESPONSE_EVENT, "cancel");
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
                    EventDispatcher.getInstance().sendResponse(message, response);
                } else if ("error".equals(response)) {
                    EventDispatcher.getInstance().sendError(message, response);
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

            final Bundle bundle = message.toBundle();
            checkBundle(bundle);
            checkBundle(bundle.getBundle("object"));
            fAssertNotSame("optBundle returns property value if it exists",
                    null, message.optBundle("object", null));
            fAssertSame("optBundle returns fallback value if property does not exist",
                    null, message.optBundle("nonexistent_object", null));

        } else if (NATIVE_RESPONSE_EVENT.equals(event)) {
            final String response = message.getString("response");
            if ("success".equals(response)) {
                callback.sendSuccess(response);
            } else if ("error".equals(response)) {
                callback.sendError(response);
            } else if ("cancel".equals(response)) {
                callback.sendCancel();
            } else {
                fFail("Response type should be valid: " + response);
            }

        } else {
            fFail("Event type should be valid: " + event);
        }
    }

    private void checkBundle(final Bundle bundle) {
        fAssertEquals("Bundle boolean has correct value", true, bundle.getBoolean("boolean"));
        fAssertEquals("Bundle int has correct value", 1, bundle.getInt("int"));
        fAssertEquals("Bundle double has correct value", 0.5, bundle.getDouble("double"));
        fAssertEquals("Bundle string has correct value", "foo", bundle.getString("string"));
    }

    private void checkJSONObject(final JSONObject object) throws JSONException {
        fAssertEquals("JSON boolean has correct value", true, object.getBoolean("boolean"));
        fAssertEquals("JSON int has correct value", 1, object.getInt("int"));
        fAssertEquals("JSON double has correct value", 0.5, object.getDouble("double"));
        fAssertEquals("JSON string has correct value", "foo", object.getString("string"));
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
    }
}
