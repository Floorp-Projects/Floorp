/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.EmptyStackException;
import java.util.Stack;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;

/* Reads out of a multiple level deep jar file such as
 *  jar:jar:file:///data/app/org.mozilla.fennec.apk!/omni.ja!/chrome/chrome/content/branding/favicon32.png
 */
public final class GeckoJarReader {
    private static final String LOGTAG = "GeckoJarReader";

    private GeckoJarReader() {}

    public static Bitmap getBitmap(Resources resources, String url) {
        BitmapDrawable drawable = getBitmapDrawable(resources, url);
        return (drawable != null) ? drawable.getBitmap() : null;
    }

    public static BitmapDrawable getBitmapDrawable(Resources resources, String url) {
        Stack<String> jarUrls = parseUrl(url);
        InputStream inputStream = null;
        BitmapDrawable bitmap = null;

        ZipFile zip = null;
        try {
            // Load the initial jar file as a zip
            zip = getZipFile(jarUrls.pop());
            inputStream = getStream(zip, jarUrls);
            if (inputStream != null) {
                bitmap = new BitmapDrawable(resources, inputStream);
            }
        } catch (IOException ex) {
            Log.e(LOGTAG, "Exception ", ex);
        } finally {
            if (inputStream != null) {
                try {
                    inputStream.close();
                } catch(IOException ex) {
                    Log.e(LOGTAG, "Error closing stream", ex);
                }
            }
            if (zip != null) {
                try {
                    zip.close();
                } catch(IOException ex) {
                    Log.e(LOGTAG, "Error closing zip", ex);
                }
            }
        }

        return bitmap;
    }

    public static String getText(String url) {
        Stack<String> jarUrls = parseUrl(url);

        ZipFile zip = null;
        BufferedReader reader = null;
        String text = null;
        try {
            zip = getZipFile(jarUrls.pop());
            InputStream input = getStream(zip, jarUrls);
            if (input != null) {
                reader = new BufferedReader(new InputStreamReader(input));
                text = reader.readLine();
            }
        } catch (IOException ex) {
            Log.e(LOGTAG, "Exception ", ex);
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch(IOException ex) {
                    Log.e(LOGTAG, "Error closing reader", ex);
                }
            }
            if (zip != null) {
                try {
                    zip.close();
                } catch(IOException ex) {
                    Log.e(LOGTAG, "Error closing zip", ex);
                }
            }
        }

        return text;
    }

    private static ZipFile getZipFile(String url) throws IOException {
        URL fileUrl = new URL(url);
        File file = new File(fileUrl.getPath());
        return new ZipFile(file);
    }

    private static InputStream getStream(ZipFile zip, Stack<String> jarUrls) throws IOException {
        ZipInputStream inputStream = null;
        ZipEntry entry = null;
        try {
            // loop through children jar files until we reach the innermost one
            while (jarUrls.peek() != null) {
                String fileName = jarUrls.pop();

                if (inputStream != null) {
                    entry = getEntryFromStream(inputStream, fileName);
                } else {
                    entry = zip.getEntry(fileName);
                }

                if (entry == null) {
                    Log.d(LOGTAG, "No Entry for " + fileName);
                    return null;
                }

                // if there is nothing else on the stack, this will throw and break us out of the loop
                jarUrls.peek();

                if (inputStream != null) {
                    inputStream = new ZipInputStream(inputStream);
                } else {
                    inputStream = new ZipInputStream(zip.getInputStream(entry));
                }
            }
        } catch (EmptyStackException ex) {
            Log.d(LOGTAG, "Jar reader reached end of stack");
        }
        return inputStream;
    }

    /* Searches through a ZipInputStream for an entry with a given name */
    private static ZipEntry getEntryFromStream(ZipInputStream zipStream, String entryName) {
        ZipEntry entry = null;

        try {
            entry = zipStream.getNextEntry();
            while(entry != null && !entry.getName().equals(entryName)) {
                entry = zipStream.getNextEntry();
            }
        } catch (IOException ex) {
            Log.e(LOGTAG, "Exception getting stream entry", ex);
        }

        return entry;
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
            results.push(url.substring(jarEnd+2)); // remove the !/ characters
            return parseUrl(subStr, results);
        } else {
            results.push(url);
            return results;
        }
    }
}
