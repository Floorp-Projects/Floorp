/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.Log;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.mozglue.NativeZip;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Stack;

/* Reads out of a multiple level deep jar file such as
 *  jar:jar:file:///data/app/org.mozilla.fennec.apk!/omni.ja!/chrome/chrome/content/branding/favicon32.png
 */
public final class GeckoJarReader {
    private static final String LOGTAG = "GeckoJarReader";

    private GeckoJarReader() {}

    public static Bitmap getBitmap(Context context, Resources resources, String url) {
        BitmapDrawable drawable = getBitmapDrawable(context, resources, url);
        return (drawable != null) ? drawable.getBitmap() : null;
    }

    public static BitmapDrawable getBitmapDrawable(Context context, Resources resources,
                                                   String url) {
        Stack<String> jarUrls = parseUrl(url);
        InputStream inputStream = null;
        BitmapDrawable bitmap = null;

        NativeZip zip = null;
        try {
            // Load the initial jar file as a zip
            zip = getZipFile(context, jarUrls.pop());
            inputStream = getStream(zip, jarUrls, url);
            if (inputStream != null) {
                bitmap = new BitmapDrawable(resources, inputStream);
                // BitmapDrawable created from a stream does not set the correct target density from resources.
                // In fact it discards the resources https://android.googlesource.com/platform/frameworks/base/+/refs/heads/master/graphics/java/android/graphics/drawable/BitmapDrawable.java#191
                bitmap.setTargetDensity(resources.getDisplayMetrics());
            }
        } catch (IOException | URISyntaxException ex) {
            Log.e(LOGTAG, "Exception ", ex);
        } finally {
            if (inputStream != null) {
                try {
                    inputStream.close();
                } catch (IOException ex) {
                    Log.e(LOGTAG, "Error closing stream", ex);
                }
            }
        }

        return bitmap;
    }

    public static String getText(Context context, String url) {
        Stack<String> jarUrls = parseUrl(url);

        NativeZip zip = null;
        BufferedReader reader = null;
        String text = null;
        try {
            zip = getZipFile(context, jarUrls.pop());
            InputStream input = getStream(zip, jarUrls, url);
            if (input != null) {
                reader = new BufferedReader(new InputStreamReader(input));
                text = reader.readLine();
            }
        } catch (IOException | URISyntaxException ex) {
            Log.e(LOGTAG, "Exception ", ex);
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException ex) {
                    Log.e(LOGTAG, "Error closing reader", ex);
                }
            }
        }

        return text;
    }

    private static NativeZip getZipFile(Context context, String url)
            throws IOException, URISyntaxException {
        URI fileUrl = new URI(url);
        GeckoLoader.loadMozGlue(context);
        return new NativeZip(fileUrl.getPath());
    }

    @RobocopTarget
    /**
     * Extract a (possibly nested) file from an archive and write it to a temporary file.
     *
     * @param context Android context.
     * @param url to open.  Can include jar: to "reach into" nested archives.
     * @param dir to write temporary file to.
     * @return a <code>File</code>, if one could be written; otherwise null.
     * @throws IOException if an error occured.
     */
    public static File extractStream(Context context, String url, File dir, String suffix) throws IOException {
        InputStream input = null;
        try {
            try {
                final URI fileURI = new URI(url);
                // We don't check the scheme because we want to catch bare files, not just file:// URIs.
                // If we let bare files through, we'd try to open them as ZIP files later -- and crash in native code.
                if (fileURI != null && fileURI.getPath() != null) {
                    final File inputFile = new File(fileURI.getPath());
                    if (inputFile != null && inputFile.exists()) {
                        input = new FileInputStream(inputFile);
                    }
                }
            } catch (URISyntaxException e) {
                // Not a file:// URI.
            }
            if (input == null) {
                // No luck with file:// URI; maybe some other URI?
                input = getStream(context, url);
            }
            if (input == null) {
                // Not found!
                return null;
            }

            // n.b.: createTempFile does not in fact delete the file.
            final File file = File.createTempFile("extractStream", suffix, dir);
            OutputStream output = null;
            try {
                output = new FileOutputStream(file);
                byte[] buf = new byte[8192];
                int len;
                while ((len = input.read(buf)) >= 0) {
                    output.write(buf, 0, len);
                }
                return file;
            } finally {
                if (output != null) {
                    output.close();
                }
            }
        } finally {
            if (input != null) {
                try {
                    input.close();
                } catch (IOException e) {
                    Log.w(LOGTAG, "Got exception closing stream; ignoring.", e);
                }
            }
        }
    }

    @RobocopTarget
    public static InputStream getStream(Context context, String url) {
        Stack<String> jarUrls = parseUrl(url);
        try {
            NativeZip zip = getZipFile(context, jarUrls.pop());
            return getStream(zip, jarUrls, url);
        } catch (Exception ex) {
            // Some JNI code throws IllegalArgumentException on a bad file name;
            // swallow the error and return null.  We could also see legitimate
            // IOExceptions here.
            Log.e(LOGTAG, "Exception getting input stream from jar URL: " + url, ex);
            return null;
        }
    }

    private static InputStream getStream(NativeZip zip, Stack<String> jarUrls, String origUrl) {
        InputStream inputStream = null;

        // loop through children jar files until we reach the innermost one
        while (!jarUrls.empty()) {
            String fileName = jarUrls.pop();

            if (inputStream != null) {
                // intermediate NativeZips and InputStreams will be garbage collected.
                try {
                    zip = new NativeZip(inputStream);
                } catch (IllegalArgumentException e) {
                    String description = "!!! BUG 849589 !!! origUrl=" + origUrl;
                    Log.e(LOGTAG, description, e);
                    throw new IllegalArgumentException(description);
                }
            }

            inputStream = zip.getInputStream(fileName);
            if (inputStream == null) {
                Log.d(LOGTAG, "No Entry for " + fileName);
                return null;
            }
        }

        return inputStream;
    }

    /* Returns a stack of strings breaking the url up into pieces. Each piece
     * is assumed to point to a jar file except for the final one. Callers should
     * pass in the url to parse, and null for the parent parameter (used for recursion)
     * For example, jar:jar:file:///data/app/org.mozilla.fennec.apk!/omni.ja!/chrome/chrome/content/branding/favicon32.png
     * will return:
     *    file:///data/app/org.mozilla.fennec.apk
     *    omni.ja
     *    chrome/chrome/content/branding/favicon32.png
     */
    private static Stack<String> parseUrl(String url) {
        return parseUrl(url, null);
    }

    private static Stack<String> parseUrl(String url, Stack<String> results) {
        if (results == null) {
            results = new Stack<String>();
        }

        if (url.startsWith("jar:")) {
            int jarEnd = url.lastIndexOf("!");
            String subStr = url.substring(4, jarEnd);
            results.push(url.substring(jarEnd + 2)); // remove the !/ characters
            return parseUrl(subStr, results);
        } else {
            results.push(url);
            return results;
        }
    }

    public static String getJarURL(Context context, String pathInsideJAR) {
        // We need to encode the package resource path, because it might contain illegal characters. For example:
        //   /mnt/asec2/[2]org.mozilla.fennec-1/pkg.apk
        // The round-trip through a URI does this for us.
        final String resourcePath = context.getPackageResourcePath();
        return computeJarURI(resourcePath, pathInsideJAR);
    }

    /**
     * Encodes its resource path correctly.
     */
    @RobocopTarget
    public static String computeJarURI(String resourcePath, String pathInsideJAR) {
        final String resURI = new File(resourcePath).toURI().toString();

        // TODO: do we need to encode the file path, too?
        return "jar:jar:" + resURI + "!/" + AppConstants.OMNIJAR_NAME + "!/" + pathInsideJAR;
    }
}
