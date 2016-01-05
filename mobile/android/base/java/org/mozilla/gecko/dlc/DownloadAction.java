/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.support.v4.net.ConnectivityManagerCompat;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.IOUtils;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.GZIPInputStream;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpStatus;
import ch.boye.httpclientandroidlib.client.HttpClient;
import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpRequestRetryHandler;
import ch.boye.httpclientandroidlib.impl.client.HttpClientBuilder;

/**
 * Download content that has been scheduled during "study" or "verify".
 */
public class DownloadAction extends BaseAction {
    private static final String LOGTAG = "DLCDownloadAction";

    private static final String CACHE_DIRECTORY = "downloadContent";
    private static final String CDN_BASE_URL = "https://mobile.cdn.mozilla.net/";

    public interface Callback {
        void onContentDownloaded(DownloadContent content);
    }

    private Callback callback;

    public DownloadAction(Callback callback) {
        this.callback = callback;
    }

    public void perform(Context context, DownloadContentCatalog catalog) {
        Log.d(LOGTAG, "Downloading content..");

        if (!isConnectedToNetwork(context)) {
            Log.d(LOGTAG, "No connected network available. Postponing download.");
            // TODO: Reschedule download (bug 1209498)
            return;
        }

        if (isActiveNetworkMetered(context)) {
            Log.d(LOGTAG, "Network is metered. Postponing download.");
            // TODO: Reschedule download (bug 1209498)
            return;
        }

        final HttpClient client = buildHttpClient();

        for (DownloadContent content : catalog.getScheduledDownloads()) {
            Log.d(LOGTAG, "Downloading: " + content);

            File temporaryFile = null;

            try {
                File destinationFile = getDestinationFile(context, content);
                if (destinationFile.exists() && verify(destinationFile, content.getChecksum())) {
                    Log.d(LOGTAG, "Content already exists and is up-to-date.");
                    catalog.markAsDownloaded(content);
                    continue;
                }

                temporaryFile = createTemporaryFile(context, content);

                if (!hasEnoughDiskSpace(content, destinationFile, temporaryFile)) {
                    Log.d(LOGTAG, "Not enough disk space to save content. Skipping download.");
                    continue;
                }

                // TODO: Check space on disk before downloading content (bug 1220145)
                final String url = createDownloadURL(content);

                if (!temporaryFile.exists() || temporaryFile.length() < content.getSize()) {
                    download(client, url, temporaryFile);
                }

                if (!verify(temporaryFile, content.getDownloadChecksum())) {
                    Log.w(LOGTAG, "Wrong checksum after download, content=" + content.getId());
                    temporaryFile.delete();
                    continue;
                }

                if (!content.isAssetArchive()) {
                    Log.e(LOGTAG, "Downloaded content is not of type 'asset-archive': " + content.getType());
                    temporaryFile.delete();
                    continue;
                }

                extract(temporaryFile, destinationFile, content.getChecksum());

                catalog.markAsDownloaded(content);

                Log.d(LOGTAG, "Successfully downloaded: " + content);

                if (callback != null) {
                    callback.onContentDownloaded(content);
                }

                if (temporaryFile != null && temporaryFile.exists()) {
                    temporaryFile.delete();
                }
            } catch (RecoverableDownloadContentException e) {
                Log.w(LOGTAG, "Downloading content failed (Recoverable): " + content , e);

                if (e.shouldBeCountedAsFailure()) {
                    catalog.rememberFailure(content, e.getErrorType());
                }

                // TODO: Reschedule download (bug 1209498)
            } catch (UnrecoverableDownloadContentException e) {
                Log.w(LOGTAG, "Downloading content failed (Unrecoverable): " + content, e);

                catalog.markAsPermanentlyFailed(content);

                if (temporaryFile != null && temporaryFile.exists()) {
                    temporaryFile.delete();
                }
            }
        }

        Log.v(LOGTAG, "Done");
    }

    protected void download(HttpClient client, String source, File temporaryFile)
            throws RecoverableDownloadContentException, UnrecoverableDownloadContentException {
        InputStream inputStream = null;
        OutputStream outputStream = null;

        final HttpGet request = new HttpGet(source);

        final long offset = temporaryFile.exists() ? temporaryFile.length() : 0;
        if (offset > 0) {
            request.setHeader("Range", "bytes=" + offset + "-");
        }

        try {
            final HttpResponse response = client.execute(request);
            final int status = response.getStatusLine().getStatusCode();
            if (status != HttpStatus.SC_OK && status != HttpStatus.SC_PARTIAL_CONTENT) {
                // We are trying to be smart and only retry if this is an error that might resolve in the future.
                // TODO: This is guesstimating at best. We want to implement failure counters (Bug 1215106).
                if (status >= 500) {
                    // Recoverable: Server errors 5xx
                    throw new RecoverableDownloadContentException(RecoverableDownloadContentException.SERVER,
                                                                  "(Recoverable) Download failed. Status code: " + status);
                } else if (status >= 400) {
                    // Unrecoverable: Client errors 4xx - Unlikely that this version of the client will ever succeed.
                    throw new UnrecoverableDownloadContentException("(Unrecoverable) Download failed. Status code: " + status);
                } else {
                    // Informational 1xx: They have no meaning to us.
                    // Successful 2xx: We don't know how to handle anything but 200.
                    // Redirection 3xx: HttpClient should have followed redirects if possible. We should not see those errors here.
                    throw new UnrecoverableDownloadContentException("(Unrecoverable) Download failed. Status code: " + status);
                }
            }

            final HttpEntity entity = response.getEntity();
            if (entity == null) {
                // Recoverable: Should not happen for a valid asset
                throw new RecoverableDownloadContentException(RecoverableDownloadContentException.SERVER, "Null entity");
            }

            inputStream = new BufferedInputStream(entity.getContent());
            outputStream = openFile(temporaryFile, status == HttpStatus.SC_PARTIAL_CONTENT);

            IOUtils.copy(inputStream, outputStream);

            inputStream.close();
            outputStream.close();
        } catch (IOException e) {
            // Recoverable: Just I/O discontinuation
            throw new RecoverableDownloadContentException(RecoverableDownloadContentException.NETWORK, e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
            IOUtils.safeStreamClose(outputStream);
        }
    }

    protected OutputStream openFile(File file, boolean append) throws FileNotFoundException {
        return new BufferedOutputStream(new FileOutputStream(file, append));
    }

    protected void extract(File sourceFile, File destinationFile, String checksum)
            throws UnrecoverableDownloadContentException, RecoverableDownloadContentException {
        InputStream inputStream = null;
        OutputStream outputStream = null;
        File temporaryFile = null;

        try {
            File destinationDirectory = destinationFile.getParentFile();
            if (!destinationDirectory.exists() && !destinationDirectory.mkdirs()) {
                throw new IOException("Destination directory does not exist and cannot be created");
            }

            temporaryFile = new File(destinationDirectory, destinationFile.getName() + ".tmp");

            inputStream = new GZIPInputStream(new BufferedInputStream(new FileInputStream(sourceFile)));
            outputStream = new BufferedOutputStream(new FileOutputStream(temporaryFile));

            IOUtils.copy(inputStream, outputStream);

            inputStream.close();
            outputStream.close();

            if (!verify(temporaryFile, checksum)) {
                Log.w(LOGTAG, "Checksum of extracted file does not match.");
                return;
            }

            move(temporaryFile, destinationFile);
        } catch (IOException e) {
            // We could not extract to the destination: Keep temporary file and try again next time we run.
            throw new RecoverableDownloadContentException(RecoverableDownloadContentException.DISK_IO, e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
            IOUtils.safeStreamClose(outputStream);

            if (temporaryFile != null && temporaryFile.exists()) {
                temporaryFile.delete();
            }
        }
    }

    protected boolean isConnectedToNetwork(Context context) {
        ConnectivityManager manager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo networkInfo = manager.getActiveNetworkInfo();

        return networkInfo != null && networkInfo.isConnected();
    }

    protected boolean isActiveNetworkMetered(Context context) {
        return ConnectivityManagerCompat.isActiveNetworkMetered(
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE));
    }

    protected HttpClient buildHttpClient() {
        // TODO: Implement proxy support (Bug 1209496)
        return HttpClientBuilder.create()
                .setUserAgent(HardwareUtils.isTablet() ?
                        AppConstants.USER_AGENT_FENNEC_TABLET :
                        AppConstants.USER_AGENT_FENNEC_MOBILE)
                .setRetryHandler(new DefaultHttpRequestRetryHandler())
                .build();
    }

    protected String createDownloadURL(DownloadContent content) {
        return CDN_BASE_URL + content.getLocation();
    }

    protected File createTemporaryFile(Context context, DownloadContent content)
            throws RecoverableDownloadContentException {
        File cacheDirectory = new File(context.getCacheDir(), CACHE_DIRECTORY);

        if (!cacheDirectory.exists() && !cacheDirectory.mkdirs()) {
            // Recoverable: File system might not be mounted NOW and we didn't download anything yet anyways.
            throw new RecoverableDownloadContentException(RecoverableDownloadContentException.DISK_IO,
                                                          "Could not create cache directory: " + cacheDirectory);
        }

        return new File(cacheDirectory, content.getDownloadChecksum() + "-" + content.getId());
    }

    protected void move(File temporaryFile, File destinationFile)
            throws RecoverableDownloadContentException, UnrecoverableDownloadContentException {
        if (!temporaryFile.renameTo(destinationFile)) {
            Log.d(LOGTAG, "Could not move temporary file to destination. Trying to copy..");
            copy(temporaryFile, destinationFile);
            temporaryFile.delete();
        }
    }

    protected void copy(File temporaryFile, File destinationFile)
            throws RecoverableDownloadContentException, UnrecoverableDownloadContentException {
        InputStream inputStream = null;
        OutputStream outputStream = null;

        try {
            File destinationDirectory = destinationFile.getParentFile();
            if (!destinationDirectory.exists() && !destinationDirectory.mkdirs()) {
                throw new IOException("Destination directory does not exist and cannot be created");
            }

            inputStream = new BufferedInputStream(new FileInputStream(temporaryFile));
            outputStream = new BufferedOutputStream(new FileOutputStream(destinationFile));

            IOUtils.copy(inputStream, outputStream);

            inputStream.close();
            outputStream.close();
        } catch (IOException e) {
            // We could not copy the temporary file to its destination: Keep the temporary file and
            // try again the next time we run.
            throw new RecoverableDownloadContentException(RecoverableDownloadContentException.DISK_IO, e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
            IOUtils.safeStreamClose(outputStream);
        }
    }

    protected boolean hasEnoughDiskSpace(DownloadContent content, File destinationFile, File temporaryFile) {
        final File temporaryDirectory = temporaryFile.getParentFile();
        if (temporaryDirectory.getUsableSpace() < content.getSize()) {
            return false;
        }

        final File destinationDirectory = destinationFile.getParentFile();
        // We need some more space to extract the file (getSize() returns the uncompressed size)
        if (destinationDirectory.getUsableSpace() < content.getSize() * 2) {
            return false;
        }

        return true;
    }
}
