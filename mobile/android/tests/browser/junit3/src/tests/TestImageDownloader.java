/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.browser.tests;

import android.content.Context;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.net.Uri;
import android.test.mock.MockResources;
import android.test.RenamingDelegatingContext;
import android.util.DisplayMetrics;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.home.ImageLoader.ImageDownloader;

public class TestImageDownloader extends BrowserTestCase {
    private static class TestContext extends RenamingDelegatingContext {
        private static final String PREFIX = "TestImageDownloader-";

        private final Resources resources;
        private final Set<String> usedPrefs;

        public TestContext(Context context) {
            super(context, PREFIX);
            resources = new TestResources();
            usedPrefs = Collections.synchronizedSet(new HashSet<String>());
        }

        @Override
        public Resources getResources() {
            return resources;
        }

        @Override
        public SharedPreferences getSharedPreferences(String name, int mode) {
            usedPrefs.add(name);
            return super.getSharedPreferences(PREFIX + name, mode);
        }

        public void clearUsedPrefs() {
            for (String prefsName : usedPrefs) {
                getSharedPreferences(prefsName, 0).edit().clear().commit();
            }

            usedPrefs.clear();
        }
    }

    private static class TestResources extends MockResources {
        private final DisplayMetrics metrics;

        public TestResources() {
            metrics = new DisplayMetrics();
        }

        @Override
        public DisplayMetrics getDisplayMetrics() {
            return metrics;
        }

        public void setDensityDpi(int densityDpi) {
            metrics.densityDpi = densityDpi;
        }
    }

    private static class TestDistribution extends Distribution {
        final List<String> accessedFiles;

        public TestDistribution(Context context) {
            super(context);
            accessedFiles = new ArrayList<String>();
        }

        @Override
        public File getDistributionFile(String name) {
            accessedFiles.add(name);

            // Return null to ensure the ImageDownloader will go
            // through a complete density lookup for each filename.
            return null;
        }

        public List<String> getAccessedFiles() {
            return Collections.unmodifiableList(accessedFiles);
        }

        public void resetAccessedFiles() {
            accessedFiles.clear();
        }
    }

    private TestContext context;
    private TestResources resources;
    private TestDistribution distribution;
    private ImageDownloader downloader;

    protected void setUp() {
        context = new TestContext(getApplicationContext());
        resources = (TestResources) context.getResources();
        distribution = new TestDistribution(context);
        downloader = new ImageDownloader(context, distribution);
    }

    protected void tearDown() {
        context.clearUsedPrefs();
    }

    private void triggerLoad(Uri uri) {
        try {
            downloader.load(uri, false);
        } catch (IOException e) {
            // Ignore any IO exceptions.
        }
    }

    private void checkAccessedFiles(String[] filenames) {
        List<String> accessedFiles = distribution.getAccessedFiles();

        for (int i = 0; i < filenames.length; i++) {
            assertEquals(filenames[i], accessedFiles.get(i));
        }
    }

    private void checkAccessedFilesForUri(Uri uri, int densityDpi, String[] filenames) {
        resources.setDensityDpi(densityDpi);
        triggerLoad(uri);
        checkAccessedFiles(filenames);
        distribution.resetAccessedFiles();
    }

    public void testAccessedFiles() {
        // Filename only.
        checkAccessedFilesForUri(Uri.parse("gecko.distribution://file"),
                                 DisplayMetrics.DENSITY_MEDIUM,
                                 new String[] {
                                    "mdpi/file.png",
                                    "xhdpi/file.png",
                                    "hdpi/file.png"
                                 });

        // Directory and filename.
        checkAccessedFilesForUri(Uri.parse("gecko.distribution://dir/file"),
                                 DisplayMetrics.DENSITY_MEDIUM,
                                 new String[] {
                                    "dir/mdpi/file.png",
                                    "dir/xhdpi/file.png",
                                    "dir/hdpi/file.png"
                                 });

        // Sub-directories and filename.
        checkAccessedFilesForUri(Uri.parse("gecko.distribution://dir/subdir/file"),
                                 DisplayMetrics.DENSITY_MEDIUM,
                                 new String[] {
                                    "dir/subdir/mdpi/file.png",
                                    "dir/subdir/xhdpi/file.png",
                                    "dir/subdir/hdpi/file.png"
                                 });
    }

    public void testDensityLookup() {
        Uri uri = Uri.parse("gecko.distribution://file");

        // Medium density
        checkAccessedFilesForUri(uri,
                                 DisplayMetrics.DENSITY_MEDIUM,
                                 new String[] {
                                    "mdpi/file.png",
                                    "xhdpi/file.png",
                                    "hdpi/file.png"
                                 });

        checkAccessedFilesForUri(uri,
                                 DisplayMetrics.DENSITY_HIGH,
                                 new String[] {
                                    "hdpi/file.png",
                                    "xxhdpi/file.png",
                                    "xhdpi/file.png"
                                 });

        checkAccessedFilesForUri(uri,
                                 DisplayMetrics.DENSITY_XHIGH,
                                 new String[] {
                                    "xhdpi/file.png",
                                    "xxhdpi/file.png",
                                    "mdpi/file.png"
                                 });


        checkAccessedFilesForUri(uri,
                                 DisplayMetrics.DENSITY_XXHIGH,
                                 new String[] {
                                    "xxhdpi/file.png",
                                    "hdpi/file.png"
                                 });
    }
}
