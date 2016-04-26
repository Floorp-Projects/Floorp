/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.FilenameFilter;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.Scanner;

import org.json.JSONObject;
import org.mozilla.gecko.annotation.RobocopTarget;

public class FileUtils {
    private static final String LOGTAG = "GeckoFileUtils";
    /*
    * A basic Filter for checking a filename and age.
    **/
    static public class NameAndAgeFilter implements FilenameFilter {
        final private String mName;
        final private double mMaxAge;

        public NameAndAgeFilter(String name, double age) {
            mName = name;
            mMaxAge = age;
        }

        @Override
        public boolean accept(File dir, String filename) {
            if (mName == null || mName.matches(filename)) {
                File f = new File(dir, filename);

                if (mMaxAge < 0 || System.currentTimeMillis() - f.lastModified() > mMaxAge) {
                    return true;
                }
            }

            return false;
        }
    }

    @RobocopTarget
    public static void delTree(File dir, FilenameFilter filter, boolean recurse) {
        String[] files = null;

        if (filter != null) {
          files = dir.list(filter);
        } else {
          files = dir.list();
        }

        if (files == null) {
          return;
        }

        for (String file : files) {
            File f = new File(dir, file);
            delete(f, recurse);
        }
    }

    public static boolean delete(File file) throws IOException {
        return delete(file, true);
    }

    public static boolean delete(File file, boolean recurse) {
        if (file.isDirectory() && recurse) {
            // If the quick delete failed and this is a dir, recursively delete the contents of the dir
            String files[] = file.list();
            for (String temp : files) {
                File fileDelete = new File(file, temp);
                try {
                    delete(fileDelete);
                } catch (IOException ex) {
                    Log.i(LOGTAG, "Error deleting " + fileDelete.getPath(), ex);
                }
            }
        }

        // Even if this is a dir, it should now be empty and delete should work
        return file.delete();
    }

    // Shortcut to slurp a file without messing around with streams.
    public static String getFileContents(File file) throws IOException {
        Scanner scanner = null;
        try {
            scanner = new Scanner(file, "UTF-8");
            return scanner.useDelimiter("\\A").next();
        } finally {
            if (scanner != null) {
                scanner.close();
            }
        }
    }

    /**
     * A generic solution to read from an input stream in UTF-8. This function will read from the stream until it
     * is finished and close the stream - this is necessary to close the wrapping resources.
     *
     * For a higher-level method, see {@link #readStringFromSmallFile(File)}.
     *
     * Since this is generic, it may not be the most performant for your use case.
     *
     * @param bufferSize Size of the underlying buffer for read optimizations - must be > 0.
     */
    public static String readStringFromInputStreamAndCloseStream(final InputStream inputStream, final int bufferSize)
            throws IOException {
        if (bufferSize <= 0) {
            // Safe close: it's more important to alert the programmer of
            // their error than to let them catch and continue on their way.
            IOUtils.safeStreamClose(inputStream);
            throw new IllegalArgumentException("Expected buffer size larger than 0. Got: " + bufferSize);
        }

        final StringBuilder stringBuilder = new StringBuilder(bufferSize);
        final InputStreamReader reader = new InputStreamReader(inputStream, Charset.forName("UTF-8"));
        try {
            int charsRead;
            final char[] buffer = new char[bufferSize];
            while ((charsRead = reader.read(buffer, 0, bufferSize)) != -1) {
                stringBuilder.append(buffer, 0, charsRead);
            }
        } finally {
            reader.close();
        }
        return stringBuilder.toString();
    }

    /**
     * A generic solution to write a JSONObject to a file.
     * See {@link #writeStringToFile(File, String)} for more details.
     */
    public static void writeJSONObjectToFile(final File file, final JSONObject obj) throws IOException {
        writeStringToFile(file, obj.toString());
    }

    /**
     * A generic solution to write to a File - the given file will be overwritten.
     * See {@link #writeStringToOutputStreamAndCloseStream(OutputStream, String)} for more details.
     */
    public static void writeStringToFile(final File file, final String str) throws IOException {
        writeStringToOutputStreamAndCloseStream(new FileOutputStream(file, false), str);
    }

    /**
     * A generic solution to write to an output stream in UTF-8. The stream will be closed at the
     * completion of this method - it's necessary in order to close the wrapping resources.
     *
     * For a higher-level method, see {@link #writeStringToFile(File, String)}.
     *
     * Since this is generic, it may not be the most performant for your use case.
     */
    public static void writeStringToOutputStreamAndCloseStream(final OutputStream outputStream, final String str)
            throws IOException {
        try {
            final OutputStreamWriter writer = new OutputStreamWriter(outputStream, Charset.forName("UTF-8"));
            try {
                writer.write(str);
            } finally {
                writer.close();
            }
        } finally {
            // OutputStreamWriter.close can throw before closing the
            // underlying stream. For safety, we close here too.
            outputStream.close();
        }
    }
}
