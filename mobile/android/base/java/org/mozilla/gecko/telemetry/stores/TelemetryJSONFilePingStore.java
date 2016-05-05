/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.stores;

import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.VisibleForTesting;
import android.util.Log;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.telemetry.TelemetryPing;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.StringUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.nio.channels.FileLock;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Locale;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * An implementation of TelemetryPingStore that is backed by JSON files.
 *
 * This implementation seeks simplicity. Each ping to upload is stored in its own file with a
 * name patterned after {@link #FILENAME}. The file name includes the ping's unique id - these are used to
 * both remove pings and prune pings. During prune, the pings with the smallest ids will be removed first
 * so these ping ids should be strictly increasing as new pings are stored; consider using data already contained
 * in the ping, such as a sequence number.
 *
 * Using separate files for this store allows for less restrictive concurrency:
 *   * requires locking: {@link #storePing(TelemetryPing)} writes a new file
 *   * requires locking: {@link #getAllPings()} reads all files, including those potentially being written,
 * hence locking
 *   * no locking: {@link #maybePrunePings()} deletes the least recently written pings, none of which should
 * be currently written
 *   * no locking: {@link #onUploadAttemptComplete(Set)} deletes the given pings, none of which should be
 * currently written
 */
public class TelemetryJSONFilePingStore implements TelemetryPingStore {
    private static final String LOGTAG = StringUtils.safeSubstring(
            "Gecko" + TelemetryJSONFilePingStore.class.getSimpleName(), 0, 23);

    @VisibleForTesting static final int MAX_PING_COUNT = 40; // TODO: value.

    private static final String FILENAME = "ping-%s.json";
    private static final Pattern FILENAME_PATTERN = Pattern.compile("ping-([0-9]+)\\.json");

    // We keep the key names short to reduce storage size impact.
    @VisibleForTesting static final String KEY_PAYLOAD = "p";
    @VisibleForTesting static final String KEY_URL_PATH = "u";

    private final File storeDir;

    public TelemetryJSONFilePingStore(final File storeDir) {
        this.storeDir = storeDir;
        this.storeDir.mkdirs();
    }

    @VisibleForTesting File getPingFile(final long id) {
        final String filename = String.format(Locale.US, FILENAME, id);
        return new File(storeDir, filename);
    }

    /**
     * @return the ID from the filename or -1 if the id does not exist.
     */
    @VisibleForTesting static int getIDFromFilename(final String filename) {
        final Matcher matcher = FILENAME_PATTERN.matcher(filename);
        if (!matcher.matches()) { // Matcher.matches has the side effect of starting the match - needed to call Matcher.group
            return -1;
        }
        return Integer.parseInt(matcher.group(1));
    }

    @Override
    public void storePing(final TelemetryPing ping) throws IOException {
        final String output;
        try {
            output = new JSONObject()
                    .put(KEY_PAYLOAD, ping.getPayload())
                    .put(KEY_URL_PATH, ping.getURLPath())
                    .toString();
        } catch (final JSONException e) {
            // Do not log the exception to avoid leaking personal data.
            throw new IOException("Unable to create JSON to store to disk");
        }

        final FileOutputStream outputStream = new FileOutputStream(getPingFile(ping.getUniqueID()), false);
        blockForLockAndWriteFileAndCloseStream(outputStream, output);
    }

    @Override
    public void maybePrunePings() {
        final File[] files = storeDir.listFiles(new PingFileFilter());
        if (files.length < MAX_PING_COUNT) {
            return;
        }

        final SortedSet<Integer> ids = getIDsFromFileList(files);
        removeFilesWithSmallestIDs(ids, files.length - MAX_PING_COUNT);
    }

    private static SortedSet<Integer> getIDsFromFileList(final File[] files) {
        // Since we get the ids from a file list which is probably sorted, I assume a TreeSet will get unbalanced
        // and could be inefficient. However, it sounds less complex and more efficient than the alternative: the
        // ConcurrentSkipListSet. In any case, our data is relatively small.
        final SortedSet<Integer> out = new TreeSet<>();
        for (final File file : files) {
            final int id = getIDFromFilename(file.getName());
            if (id >= 0) {
                out.add(id);
            }
        }
        return out;
    }

    private void removeFilesWithSmallestIDs(final SortedSet<Integer> ids, final int numFilesToRemove) {
        final Iterator<Integer> it = ids.iterator();
        int i = 0;
        while (i < numFilesToRemove) { // Sorted set so these are ascending values.
            i += 1;
            final Integer id = it.next(); // file count > files to remove so this should not throw.
            getPingFile(id).delete();
        }
    }

    @Override
    public ArrayList<TelemetryPing> getAllPings() {
        final File[] files = storeDir.listFiles(new PingFileFilter());
        final ArrayList<TelemetryPing> out = new ArrayList<>(files.length);
        for (final File file : files) {
            final JSONObject obj = lockAndReadJSONFromFile(file);
            if (obj == null) {
                // We log in the method to get the JSONObject if we return null.
                continue;
            }

            try {
                final String url = obj.getString(KEY_URL_PATH);
                final ExtendedJSONObject payload = new ExtendedJSONObject(obj.getString(KEY_PAYLOAD));
                final int id = getIDFromFilename(file.getName());
                if (id < 0) {
                    throw new IllegalStateException("These files are already filtered - did not expect to see " +
                            "an invalid ID in these files");
                }
                out.add(new TelemetryPing(url, payload, id));
            } catch (final IOException | JSONException | NonObjectJSONException e) {
                Log.w(LOGTAG, "Bad json in ping. Ignoring.");
                continue;
            }
        }
        return out;
    }

    /**
     * Logs if there is an error.
     *
     * @return the JSON object from the given file or null if there is an error.
     */
    private JSONObject lockAndReadJSONFromFile(final File file) {
        final FileInputStream inputStream;
        try {
            inputStream = new FileInputStream(file);
        } catch (final FileNotFoundException e) {
            throw new IllegalStateException("Expected file to exist");
        }

        final JSONObject obj;
        try {
            // Potential optimization: re-use the same buffer for reading from files.
            obj = lockAndReadFileAndCloseStream(inputStream, (int) file.length());
        } catch (final IOException | JSONException e) {
            // We couldn't read this file so let's just skip it. These potentially
            // corrupted files should be removed when the data is pruned.
            Log.w(LOGTAG, "Error when reading file: " + file.getName() + " Likely corrupted. Ignoring");
            return null;
        }

        if (obj == null) {
            Log.d(LOGTAG, "Could not read given file: " + file.getName() + " File is locked. Ignoring");
        }
        return obj;
    }

    @Override
    public void onUploadAttemptComplete(final Set<Integer> successfulRemoveIDs) {
        if (successfulRemoveIDs.isEmpty()) {
            return;
        }

        final File[] files = storeDir.listFiles(new PingFileFilter(successfulRemoveIDs));
        for (final File file : files) {
            file.delete();
        }
    }

    /**
     * Locks the given {@link FileOutputStream} and writes the given String. This method will close the given stream.
     *
     * Note: this method blocks until a file lock can be acquired.
     */
    private static void blockForLockAndWriteFileAndCloseStream(final FileOutputStream outputStream, final String str)
            throws IOException {
        try {
            final FileLock lock = outputStream.getChannel().lock(0, Long.MAX_VALUE, false);
            if (lock != null) {
                // The file lock is released when the stream is closed. If we try to redundantly close it, we get
                // a ClosedChannelException. To be safe, we could catch that every time but there is a performance
                // hit to exception handling so instead we assume the file lock will be closed.
                FileUtils.writeStringToOutputStreamAndCloseStream(outputStream, str);
            }
        } finally {
            outputStream.close(); // redundant: closed when the stream is closed, but let's be safe.
        }
    }

    /**
     * Locks the given {@link FileInputStream} and reads the data. This method will close the given stream.
     *
     * Note: this method returns null when a lock could not be acquired.
     */
    private static JSONObject lockAndReadFileAndCloseStream(final FileInputStream inputStream, final int fileSize)
            throws IOException, JSONException {
        try {
            final FileLock lock = inputStream.getChannel().tryLock(0, Long.MAX_VALUE, true); // null when lock not acquired
            if (lock == null) {
                return null;
            }
            // The file lock is released when the stream is closed. If we try to redundantly close it, we get
            // a ClosedChannelException. To be safe, we could catch that every time but there is a performance
            // hit to exception handling so instead we assume the file lock will be closed.
            return new JSONObject(FileUtils.readStringFromInputStreamAndCloseStream(inputStream, fileSize));
        } finally {
            inputStream.close(); // redundant: closed when the stream is closed, but let's be safe.
        }
    }

    private static class PingFileFilter implements FilenameFilter {
        private final Set<Integer> idsToFilter;

        public PingFileFilter() {
            this(null);
        }

        public PingFileFilter(final Set<Integer> idsToFilter) {
            this.idsToFilter = idsToFilter;
        }

        @Override
        public boolean accept(final File dir, final String filename) {
            if (idsToFilter == null) {
                return FILENAME_PATTERN.matcher(filename).matches();
            }

            return idsToFilter.contains(getIDFromFilename(filename));
        }
    }

    public static final Parcelable.Creator<TelemetryJSONFilePingStore> CREATOR = new Parcelable.Creator<TelemetryJSONFilePingStore>() {
        @Override
        public TelemetryJSONFilePingStore createFromParcel(final Parcel source) {
            final String storeDirPath = source.readString();
            return new TelemetryJSONFilePingStore(new File(storeDirPath));
        }

        @Override
        public TelemetryJSONFilePingStore[] newArray(final int size) {
            return new TelemetryJSONFilePingStore[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(final Parcel dest, final int flags) {
        dest.writeString(storeDir.getAbsolutePath());
    }
}
