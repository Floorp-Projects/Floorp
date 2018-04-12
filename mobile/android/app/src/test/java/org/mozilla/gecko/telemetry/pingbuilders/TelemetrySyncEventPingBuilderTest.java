/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.os.Bundle;

import org.json.simple.JSONArray;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.telemetry.TelemetryContract;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class TelemetrySyncEventPingBuilderTest {

    @Test
    public void testGeneralShape() throws Exception {
        JSONArray payload = buildPayloadArray(123456L, "sync", "object", "method", null, null);
        Assert.assertArrayEquals(new Object[] {123456L, "sync", "method", "object"}, payload.toArray());

        payload = buildPayloadArray(123456L, "sync", "object", "method", "value", null);
        Assert.assertArrayEquals(new Object[] {123456L, "sync", "method", "object", "value"}, payload.toArray());

        Bundle extra = new Bundle();
        extra.putString("extra-key", "extra-value");

        payload = buildPayloadArray(123456L, "sync", "object", "method", null, extra);
        Assert.assertEquals("[123456,\"sync\",\"method\",\"object\",null,{\"extra-key\":\"extra-value\"}]",
                payload.toJSONString());

        payload = buildPayloadArray(123456L, "sync", "object", "method", "value", extra);
        Assert.assertEquals("[123456,\"sync\",\"method\",\"object\",\"value\",{\"extra-key\":\"extra-value\"}]",
                payload.toJSONString());
    }

    @Test(expected = IllegalStateException.class)
    public void testNullTimestamp() throws Exception {
        buildPayloadArray(null, "category", "object", "method", null, null);
    }

    @Test(expected = IllegalStateException.class)
    public void testNullCategory() throws Exception {
        buildPayloadArray(123456L, null, "object", "method", null, null);
    }

    @Test(expected = IllegalStateException.class)
    public void testNullObject() throws Exception {
        buildPayloadArray(123456L, "category", null, "method", null, null);
    }

    @Test(expected = IllegalStateException.class)
    public void testNullMethod() throws Exception {
        buildPayloadArray(123456L, "category", "object", null, null, null);
    }

    private JSONArray buildPayloadArray(Long ts, String category, String object, String method,
                                        String value, Bundle extra) throws Exception {
        Bundle bundle = new Bundle();
        if (ts != null) {
            bundle.putLong(TelemetryContract.KEY_EVENT_TIMESTAMP, ts);
        }
        bundle.putString(TelemetryContract.KEY_EVENT_CATEGORY, category);
        bundle.putString(TelemetryContract.KEY_EVENT_OBJECT, object);
        bundle.putString(TelemetryContract.KEY_EVENT_METHOD, method);
        if (value != null) {
            bundle.putString(TelemetryContract.KEY_EVENT_VALUE, value);
        }
        if (extra != null) {
            bundle.putBundle(TelemetryContract.KEY_EVENT_EXTRA, extra);
        }
        return new TelemetrySyncEventPingBuilder().fromEventTelemetry(bundle)
                .build().getPayload().getArray("event");
    }
}
