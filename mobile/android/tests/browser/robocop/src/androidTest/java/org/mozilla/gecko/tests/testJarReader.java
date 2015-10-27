/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.InputStream;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.util.GeckoJarReader;

import android.content.Context;

/**
 * A basic jar reader test. Tests reading a png from fennec's apk, as well
 * as loading some invalid jar urls.
 */
public class testJarReader extends BaseTest {
    public void testJarReader() {
        // Invalid characters are escaped.
        final String s = GeckoJarReader.computeJarURI("some[1].apk", "something/else");
        mAsserter.ok(!s.contains("["), "Illegal characters are escaped away.", null);
        mAsserter.ok(!s.toLowerCase().contains("%2f"), "Path characters aren't escaped.", null);

        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        String appPath = getActivity().getApplication().getPackageResourcePath();
        mAsserter.isnot(appPath, null, "getPackageResourcePath is non-null");

        // Test reading a file from a jar url that looks correct.
        String url = "jar:file://" + appPath + "!/" + AppConstants.OMNIJAR_NAME;
        InputStream stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        mAsserter.isnot(stream, null, "JarReader returned non-null for valid file in valid jar");

        // Test looking for an non-existent file in a jar.
        url = "jar:file://" + appPath + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/nonexistent_file.png");
        mAsserter.is(stream, null, "JarReader returned null for non-existent file in valid jar");

        // Test looking for a file that doesn't exist in the APK.
        url = "jar:file://" + appPath + "!/" + "BAD" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        mAsserter.is(stream, null, "JarReader returned null for valid file in invalid jar file");

        // Test looking for a file that doesn't exist in the APK.
        // Bug 1174922, prefixed string / length error.
        url = "jar:file://" + appPath + "!/" + AppConstants.OMNIJAR_NAME + "BAD";
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        mAsserter.is(stream, null, "JarReader returned null for valid file in other invalid jar file");

        // Test looking for an jar with an invalid url.
        url = "jar:file://" + appPath + "!" + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/nonexistent_file.png");
        mAsserter.is(stream, null, "JarReader returned null for bad jar url");

        // Test looking for a file that doesn't exist on disk.
        url = "jar:file://" + appPath + "BAD" + "!/" + AppConstants.OMNIJAR_NAME;
        stream = GeckoJarReader.getStream(context, "jar:" + url + "!/chrome/chrome/content/branding/favicon32.png");
        mAsserter.is(stream, null, "JarReader returned null for a non-existent APK");

        // This test completes very quickly. If it completes too soon, the
        // minidumps directory may not be created before the process is
        // taken down, causing bug 722166.
        blockForGeckoReady();
    }

    private String getData(InputStream stream) {
        return new java.util.Scanner(stream).useDelimiter("\\A").next();
    }

}
