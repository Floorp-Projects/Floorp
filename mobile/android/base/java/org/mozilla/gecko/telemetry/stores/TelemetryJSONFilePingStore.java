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
import org.mozilla.gecko.util.FileUtils.FileLastModifiedComparator;
import org.mozilla.gecko.util.FileUtils.FilenameRegexFilter;
import org.mozilla.gecko.util.FileUtils.FilenameWhitelistFilter;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.UUIDUtil;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.nio.channels.FileLock;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;

/**
 * An implementation of TelemetryPingStore that is backed by JSON files.
 *
 * This implementation seeks simplicity. Each ping to upload is stored in its own file with its doc ID
 * as the filename. The doc ID is sent with a ping to be uploaded and is expected to be returned with
 * {@link #onUploadAttemptComplete(Set)} so the associated file can be removed.
 *
 * During prune, the pings with the oldest modified time will be removed first. Different filesystems will
 * handle clock skew (e.g. manual time changes, daylight savings time, changing timezones) in different ways
 * and we accept that these modified times may not be consistent - newer data is not more important than
 * older data and the choice to delete the oldest data first is largely arbitrary so we don't care if
 * the timestamps are occasionally inconsistent.
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

    // We keep the key names short to reduce storage size impact.
    @VisibleForTesting static final String KEY_PAYLOAD = "p";
    @VisibleForTesting static final String KEY_URL_PATH = "u";

    private final File storeDir;
    private final FilenameFilter uuidFilenameFilter;
    private final FileLastModifiedComparator fileLastModifiedComparator = new FileLastModifiedComparator();

    public TelemetryJSONFilePingStore(final File storeDir) {
        this.storeDir = storeDir;
        this.storeDir.mkdirs();
        uuidFilenameFilter = new FilenameRegexFilter(UUIDUtil.UUID_PATTERN);
    }

    @VisibleForTesting File getPingFile(final String docID) {
        return new File(storeDir, docID);
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

        final FileOutputStream outputStream = new FileOutputStream(getPingFile(ping.getDocID()), false);
        blockForLockAndWriteFileAndCloseStream(outputStream, output);
    }

    @Override
    public void maybePrunePings() {
        final File[] files = storeDir.listFiles(uuidFilenameFilter);
        if (files.length < MAX_PING_COUNT) {
            return;
        }

        final SortedSet<File> sortedFiles = new TreeSet<>(fileLastModifiedComparator);
        sortedFiles.addAll(Arrays.asList(files));
        deleteSmallestFiles(sortedFiles, files.length - MAX_PING_COUNT);
    }

    private void deleteSmallestFiles(final SortedSet<File> files, final int numFilesToRemove) {
        final Iterator<File> it = files.iterator();
        int i = 0;
        while (i < numFilesToRemove) {
            i += 1;
            // Sorted set so we're iterating over ascending files.
            final File file = it.next(); // file count > files to remove so this should not throw.
            file.delete();
        }
    }

    @Override
    public ArrayList<TelemetryPing> getAllPings() {
        final List<File> files = Arrays.asList(storeDir.listFiles(uuidFilenameFilter));
        Collections.sort(files, fileLastModifiedComparator); // oldest to newest
        final ArrayList<TelemetryPing> out = new ArrayList<>(files.size());
        for (final File file : files) {
            final JSONObject obj = lockAndReadJSONFromFile(file);
            if (obj == null) {
                // We log in the method to get the JSONObject if we return null.
                continue;
            }

            try {
                final String url = obj.getString(KEY_URL_PATH);
                final ExtendedJSONObject payload = new ExtendedJSONObject(obj.getString(KEY_PAYLOAD));
                out.add(new TelemetryPing(url, payload, file.getName()));
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
        // lockAndReadFileAndCloseStream doesn't handle file size of 0.
        if (file.length() == 0) {
            Log.w(LOGTAG, "Unexpected empty file: " + file.getName() + ". Ignoring");
            return null;
        }

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
    public void onUploadAttemptComplete(final Set<String> successfulRemoveIDs) {
        if (successfulRemoveIDs.isEmpty()) {
            return;
        }

        final File[] files = storeDir.listFiles(new FilenameWhitelistFilter(successfulRemoveIDs));
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
