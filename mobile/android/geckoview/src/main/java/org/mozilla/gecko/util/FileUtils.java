/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.FilenameFilter;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.Comparator;
import java.util.Random;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.JSONException;
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

    /**
     * A generic solution to read a JSONObject from a file. See
     * {@link #readStringFromFile(File)} for more details.
     *
     * @throws IOException if the file is empty, or another IOException occurs
     * @throws JSONException if the file could not be converted to a JSONObject.
     */
    public static JSONObject readJSONObjectFromFile(final File file) throws IOException, JSONException {
        if (file.length() == 0) {
            // Redirect this exception so it's clearer than when the JSON parser catches it.
            throw new IOException("Given file is empty - the JSON parser cannot create an object from an empty file");
        }
        return new JSONObject(readStringFromFile(file));
    }

    /**
     * A generic solution to read from a file. For more details,
     * see {@link #readStringFromInputStreamAndCloseStream(InputStream, int)}.
     *
     * This method loads the entire file into memory so will have the expected performance impact.
     * If you're trying to read a large file, you should be handling your own reading to avoid
     * out-of-memory errors.
     */
    public static String readStringFromFile(final File file) throws IOException {
        // FileInputStream will throw FileNotFoundException if the file does not exist, but
        // File.length will return 0 if the file does not exist so we catch it sooner.
        if (!file.exists()) {
            throw new FileNotFoundException("Given file, " + file + ", does not exist");
        } else if (file.length() == 0) {
            return "";
        }
        final int len = (int) file.length(); // includes potential EOF character.
        return readStringFromInputStreamAndCloseStream(new FileInputStream(file), len);
    }

    /**
     * A generic solution to read from an input stream in UTF-8. This function will read from the stream until it
     * is finished and close the stream - this is necessary to close the wrapping resources.
     *
     * For a higher-level method, see {@link #readStringFromFile(File)}.
     *
     * Since this is generic, it may not be the most performant for your use case.
     *
     * @param bufferSize Size of the underlying buffer for read optimizations - must be &gt; 0.
     */
    public static String readStringFromInputStreamAndCloseStream(final InputStream inputStream, final int bufferSize)
            throws IOException {
        InputStreamReader reader = null;
        try {
            if (bufferSize <= 0) {
                throw new IllegalArgumentException("Expected buffer size larger than 0. Got: " + bufferSize);
            }

            final StringBuilder stringBuilder = new StringBuilder(bufferSize);
            reader = new InputStreamReader(inputStream, StringUtils.UTF_8);

            int charsRead;
            final char[] buffer = new char[bufferSize];
            while ((charsRead = reader.read(buffer, 0, bufferSize)) != -1) {
                stringBuilder.append(buffer, 0, charsRead);
            }

            return stringBuilder.toString();
        } finally {
            IOUtils.safeStreamClose(reader);
            IOUtils.safeStreamClose(inputStream);
        }
    }

    /**
     * A generic solution to write a JSONObject to a file.
     * See {@link #writeStringToFile(File, String)} for more details.
     */
    public static void writeJSONObjectToFile(final File file, final JSONObject obj) throws IOException {
        writeStringToFile(file, obj.toString());
    }

    /**
     * A generic solution to write to a File - the given file will be overwritten. If it does not exist yet, it will
     * be created. See {@link #writeStringToOutputStreamAndCloseStream(OutputStream, String)} for more details.
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

    public static class FilenameWhitelistFilter implements FilenameFilter {
        private final Set<String> mFilenameWhitelist;

        public FilenameWhitelistFilter(final Set<String> filenameWhitelist) {
            mFilenameWhitelist = filenameWhitelist;
        }

        @Override
        public boolean accept(final File dir, final String filename) {
            return mFilenameWhitelist.contains(filename);
        }
    }

    public static class FilenameRegexFilter implements FilenameFilter {
        private final Pattern mPattern;

        // Each time `Pattern.matcher` is called, a new matcher is created. We can avoid the excessive object creation
        // by caching the returned matcher and calling `Matcher.reset` on it. Since Matcher's are not thread safe,
        // this assumes `FilenameFilter.accept` is not run in parallel (which, according to the source, it is not).
        private Matcher mCachedMatcher;

        public FilenameRegexFilter(final Pattern pattern) {
            mPattern = pattern;
        }

        public FilenameRegexFilter(final String pattern) {
            mPattern = Pattern.compile(pattern);
        }

        @Override
        public boolean accept(final File dir, final String filename) {
            if (mCachedMatcher == null) {
                mCachedMatcher = mPattern.matcher(filename);
            } else {
                mCachedMatcher.reset(filename);
            }
            return mCachedMatcher.matches();
        }
    }

    public static class FileLastModifiedComparator implements Comparator<File> {
        @Override
        public int compare(final File lhs, final File rhs) {
            // Long.compare is API 19+.
            final long lhsModified = lhs.lastModified();
            final long rhsModified = rhs.lastModified();
            if (lhsModified < rhsModified) {
                return -1;
            } else if (lhsModified == rhsModified) {
                return 0;
            } else {
                return 1;
            }
        }
    }

    public static File createTempDir(File directory, String prefix) {
        // Force a prefix null check first
        if (prefix.length() < 3) {
            throw new IllegalArgumentException("prefix must be at least 3 characters");
        }
        if (directory == null) {
            String tmpDir = System.getProperty("java.io.tmpdir", ".");
            directory = new File(tmpDir);
        }
        File result;
        Random random = new Random();
        do {
            result = new File(directory, prefix + random.nextInt());
        } while (!result.mkdirs());
        return result;
    }
}
