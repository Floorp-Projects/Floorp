/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.util;

import static org.junit.Assert.*;

import android.net.Uri;
import android.os.Parcel;
import android.test.suitebuilder.annotation.SmallTest;

import org.json.JSONException;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.List;

@RunWith(RobolectricTestRunner.class)
@SmallTest
public class IntentUtilsTest {

    @Test
    public void shouldNormalizeUri() {
        final String uri = "HTTPS://mozilla.org";
        final Uri normUri = IntentUtils.normalizeUri(uri);
        assertEquals("https://mozilla.org", normUri.toString());
    }

    @Test
    public void safeHttpUri() {
        final String uri = "https://mozilla.org";
        assertTrue(IntentUtils.isUriSafeForScheme(uri));
    }

    @Test
    public void safeIntentUri() {
        final String uri = "intent:https://mozilla.org#Intent;end;";
        assertTrue(IntentUtils.isUriSafeForScheme(uri));
    }

    @Test
    public void unsafeIntentUri() {
        final String uri = "intent:file:///storage/emulated/0/Download#Intent;end";
        assertFalse(IntentUtils.isUriSafeForScheme(uri));
    }

    @Test
    public void safeTelUri() {
        final String uri = "tel:12345678";
        assertTrue(IntentUtils.isUriSafeForScheme(uri));
    }

    @Test
    public void unsafeTelUri() {
        final String uri = "tel:#12345678";
        assertFalse(IntentUtils.isUriSafeForScheme(uri));
    }
}
