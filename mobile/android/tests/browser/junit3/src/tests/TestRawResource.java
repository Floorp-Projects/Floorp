/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.browser.tests;

import android.content.Context;
import android.content.res.Resources;
import android.test.mock.MockContext;
import android.test.mock.MockResources;
import android.util.TypedValue;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;

import org.mozilla.gecko.util.RawResource;

/**
 * Tests whether RawResource.getAsString() produces the right String
 * result after reading the returned raw resource's InputStream.
 */
public class TestRawResource extends BrowserTestCase {
    private static final int RAW_RESOURCE_ID = 1;
    private static final String RAW_CONTENTS = "RAW";

    private static class TestContext extends MockContext {
        private final Resources resources;

        public TestContext() {
            resources = new TestResources();
        }

        @Override
        public Resources getResources() {
            return resources;
        }
    }

    /**
     * Browser instrumentation tests can't have access to test-only
     * resources (bug 994135) yet so we mock the access to resources
     * for now.
     */
    private static class TestResources extends MockResources {
        @Override
        public InputStream openRawResource(int id) {
            if (id == RAW_RESOURCE_ID) {
                return new ByteArrayInputStream(RAW_CONTENTS.getBytes());
            }

            return null;
        }
    }

    public void testGet() {
        Context context = new TestContext();
        String result;

        try {
            result = RawResource.getAsString(context, RAW_RESOURCE_ID);
        } catch (IOException e) {
            result = null;
        }

        assertEquals(RAW_CONTENTS, result);
    }
}