/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.File;
import java.net.URL;
import java.util.EmptyStackException;
import java.util.Stack;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipEntry;
import java.io.InputStream;
import java.io.IOException;

import android.util.Log;

/* Reads out of a multiple level deep jar file such as
 *  jar:jar:file:///data/app/org.mozilla.fennec.apk!/omni.ja!/chrome/chrome/content/branding/favicon32.png
 */
public class GeckoJarReader {
    static private String LOGTAG = "GeckoJarReader";

    static public InputStream getStream(String url) {
        Stack<String> jarUrls = parseUrl(url);
        ZipInputStream inputStream = null;

        try {
            // Load the initial jar file as a zip
            URL fileUrl = new URL(jarUrls.pop());
            File file = new File(fileUrl.getPath());
            ZipFile zip = new ZipFile(file);
            ZipEntry entry = null;

            // loop through children jar files until we reach the innermost one
            while (jarUrls.peek() != null) {
                String fileName = jarUrls.pop();

                if (inputStream != null) {
                    entry = getEntryFromStream(inputStream, fileName);
                } else {
                    entry = zip.getEntry(fileName);
                }

                // if there is nothing else on the stack, this will throw and break us out of the loop
                jarUrls.peek();

                if (inputStream != null) {
                    inputStream = new ZipInputStream(inputStream);
                } else {
                    inputStream = new ZipInputStream(zip.getInputStream(entry));
                }
  
                if (entry == null) {
                    Log.d(LOGTAG, "No Entry for " + fileName);
                    return null;
                }
            }
        } catch (EmptyStackException ex) {
            Log.d(LOGTAG, "Reached Jar reader reached end of stack");
        } catch (IOException ex) {
            Log.e(LOGTAG, "Exception ", ex);
        } catch (Exception ex) {
            Log.e(LOGTAG, "Exception ", ex);
        }

        return inputStream;
    }

    /* Searches through a ZipInputStream for an entry with a given name */
    static private ZipEntry getEntryFromStream(ZipInputStream zipStream, String entryName) {
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
    static private Stack<String> parseUrl(String url) {
        return parseUrl(url, null);
    }

    static private Stack<String> parseUrl(String url, Stack<String> results) {
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
