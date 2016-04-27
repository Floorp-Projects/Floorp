/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.tests.browser.junit3;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Stack;

import android.test.InstrumentationTestCase;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.GeckoJarReader;

import android.content.Context;

/**
 * A basic jar reader test. Tests reading a png from fennec's apk, as well as
 * loading some invalid jar urls.
 */
public class TestJarReader extends InstrumentationTestCase {
    public void testJarReader() {
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        String appPath = getInstrumentation().getTargetContext().getPackageResourcePath();
        assertNotNull(appPath);

        // Test reading a file from a jar url that looks correct.
        String url = "jar:file://" + appPath + "!/" + AppConstants.OMNIJAR_NAME;
        InputStream stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        assertNotNull(stream);

        // Test looking for an non-existent file in a jar.
        url = "jar:file://" + appPath + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/nonexistent_file.png");
        assertNull(stream);

        // Test looking for a file that doesn't exist in the APK.
        url = "jar:file://" + appPath + "!/" + "BAD" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        assertNull(stream);

        // Test looking for a file that doesn't exist in the APK.
        // Bug 1174922, prefixed string / length error.
        url = "jar:file://" + appPath + "!/" + AppConstants.OMNIJAR_NAME + "BAD";
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        assertNull(stream);

        // Test looking for an jar with an invalid url.
        url = "jar:file://" + appPath + "!" + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/nonexistent_file.png");
        assertNull(stream);

        // Test looking for a file that doesn't exist on disk.
        url = "jar:file://" + appPath + "BAD" + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        assertNull(stream);
    }

    protected void assertExtractStream(String url) throws IOException {
        final File file = GeckoJarReader.extractStream(getInstrumentation().getTargetContext(), url, getInstrumentation().getContext().getCacheDir(), ".test");
        assertNotNull(file);
        try {
            assertTrue(file.getName().endsWith("test"));
            final String contents = FileUtils.readStringFromFile(file);
            assertNotNull(contents);
            assertTrue(contents.length() > 0);
        } finally {
            file.delete();
        }
    }

    public void testExtractStream() throws IOException {
        String appPath = getInstrumentation().getTargetContext().getPackageResourcePath();
        assertNotNull(appPath);

        // We don't have a lot of good files to choose from.  package-name.txt isn't included in Gradle APKs.
        assertExtractStream("jar:file://" + appPath + "!/resources.arsc");

        final String url = GeckoJarReader.getJarURL(getInstrumentation().getTargetContext(), "chrome.manifest");
        assertExtractStream(url);

        // Now use an extracted copy of chrome.manifest to test further.
        final File file = GeckoJarReader.extractStream(getInstrumentation().getTargetContext(), url, getInstrumentation().getContext().getCacheDir(), ".test");
        assertNotNull(file);
        try {
            assertExtractStream("file://" + file.getAbsolutePath()); // file:// URI.
            assertExtractStream(file.getAbsolutePath()); // Vanilla path.
        } finally {
            file.delete();
        }
    }

    protected void assertExtractStreamFails(String url) throws IOException {
        final File file = GeckoJarReader.extractStream(getInstrumentation().getTargetContext(), url, getInstrumentation().getContext().getCacheDir(), ".test");
        assertNull(file);
    }

    public void testExtractStreamFailureCases() throws IOException {
        String appPath = getInstrumentation().getTargetContext().getPackageResourcePath();
        assertNotNull(appPath);

        // First, a bad APK.
        assertExtractStreamFails("jar:file://" + appPath + "BAD!/resources.arsc");

        // Second, a bad file in the APK.
        assertExtractStreamFails("jar:file://" + appPath + "!/BADresources.arsc");

        // Now a bad file in the omnijar.
        final String badUrl = GeckoJarReader.getJarURL(getInstrumentation().getTargetContext(), "BADchrome.manifest");
        assertExtractStreamFails(badUrl);

        // Now use an extracted copy of chrome.manifest to test further.
        final String goodUrl = GeckoJarReader.getJarURL(getInstrumentation().getTargetContext(), "chrome.manifest");
        final File file = GeckoJarReader.extractStream(getInstrumentation().getTargetContext(), goodUrl, getInstrumentation().getContext().getCacheDir(), ".test");
        assertNotNull(file);
        try {
            assertExtractStreamFails("file://" + file.getAbsolutePath() + "BAD"); // Bad file:// URI.
            assertExtractStreamFails(file.getAbsolutePath() + "BAD"); //Bad vanilla path.
        } finally {
            file.delete();
        }
    }
}
