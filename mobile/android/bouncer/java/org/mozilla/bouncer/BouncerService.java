/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.bouncer;

import android.app.IntentService;
import android.content.Intent;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

public class BouncerService extends IntentService {

    private static final String LOGTAG = "GeckoBouncerService";

    public BouncerService() {
        super("BouncerService");
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        final byte[] buffer = new byte[8192];

        Log.d(LOGTAG, "Preparing to copy distribution files");

        final List<String> files;
        try {
            files = getFiles("distribution");
        } catch (IOException e) {
            Log.e(LOGTAG, "Error getting distribution files from assets/distribution/**", e);
            return;
        }

        InputStream in = null;
        for (String path : files) {
            try {
                Log.d(LOGTAG, "Copying distribution file: " + path);

                in = getAssets().open(path);

                final File outFile = getDataFile(path);
                writeStream(in, outFile, buffer);
            } catch (IOException e) {
                Log.e(LOGTAG, "Error opening distribution input stream from assets", e);
            } finally {
                if (in != null) {
                    try {
                        in.close();
                    } catch (IOException e) {
                        Log.e(LOGTAG, "Error closing distribution input stream", e);
                    }
                }
            }
        }
    }

    /**
     * Recursively traverse a directory to list paths to all files.
     *
     * @param path Directory to traverse.
     * @return List of all files in given directory.
     * @throws IOException
     */
    private List<String> getFiles(String path) throws IOException {
        List<String> paths = new ArrayList<>();
        getFiles(path, paths);
        return paths;
    }

    /**
     * Recursively traverse a directory to list paths to all files.
     *
     * @param path Directory to traverse.
     * @param acc Accumulator of paths seen.
     * @throws IOException
     */
    private void getFiles(String path, List<String> acc) throws IOException {
        final String[] list = getAssets().list(path);
        if (list.length > 0) {
            // We're a directory -- recurse.
            for (final String file : list) {
                getFiles(path + "/" + file, acc);
            }
        } else {
            // We're a file -- accumulate.
            acc.add(path);
        }
    }

    private String getDataDir() {
        return getApplicationInfo().dataDir;
    }

    private File getDataFile(final String path) {
        File outFile = new File(getDataDir(), path);
        File dir = outFile.getParentFile();

        if (dir != null && !dir.exists()) {
            Log.d(LOGTAG, "Creating " + dir.getAbsolutePath());
            if (!dir.mkdirs()) {
                Log.e(LOGTAG, "Unable to create directories: " + dir.getAbsolutePath());
                return null;
            }
        }

        return outFile;
    }

    private void writeStream(InputStream fileStream, File outFile, byte[] buffer)
            throws IOException {
        final OutputStream outStream = new FileOutputStream(outFile);
        try {
            int count;
            while ((count = fileStream.read(buffer)) > 0) {
                outStream.write(buffer, 0, count);
            }
        } finally {
            outStream.close();
        }
    }
}
