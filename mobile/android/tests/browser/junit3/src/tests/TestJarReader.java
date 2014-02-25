/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.browser.tests;

import java.io.InputStream;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.util.GeckoJarReader;

/**
 * A basic jar reader test. Tests reading a png from fennec's apk, as well as
 * loading some invalid jar urls.
 */
public class TestJarReader extends BrowserTestCase {
    public void testJarReader() {
        String appPath = getActivity().getApplication().getPackageResourcePath();
        assertNotNull(appPath);

        // Test reading a file from a jar url that looks correct.
        String url = "jar:file://" + appPath + "!/" + AppConstants.OMNIJAR_NAME;
        InputStream stream = GeckoJarReader.getStream("jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        assertNotNull(stream);

        // Test looking for an non-existent file in a jar.
        url = "jar:file://" + appPath + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream("jar:" + url + "!/chrome/chrome/content/branding/nonexistent_file.png");
        assertNull(stream);

        // Test looking for a file that doesn't exist in the APK.
        url = "jar:file://" + appPath + "!/" + "BAD" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream("jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        assertNull(stream);

        // Test looking for an jar with an invalid url.
        url = "jar:file://" + appPath + "!" + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream("jar:" + url + "!/chrome/chrome/content/branding/nonexistent_file.png");
        assertNull(stream);

        // Test looking for a file that doesn't exist on disk.
        url = "jar:file://" + appPath + "BAD" + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream("jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        assertNull(stream);
    }
}
