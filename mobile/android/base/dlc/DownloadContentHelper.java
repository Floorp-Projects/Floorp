/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.background.nativecode.NativeCrypto;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.IOUtils;

import android.content.Context;
import android.net.ConnectivityManager;
import android.support.v4.net.ConnectivityManagerCompat;
import android.util.Log;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpStatus;
import ch.boye.httpclientandroidlib.client.HttpClient;
import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpRequestRetryHandler;
import ch.boye.httpclientandroidlib.impl.client.HttpClientBuilder;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.NoSuchAlgorithmException;
import java.util.zip.GZIPInputStream;

/* package-private */ class DownloadContentHelper {
    private static final String LOGTAG = "GeckoDLCHelper";

    public static final String ACTION_STUDY_CATALOG = AppConstants.ANDROID_PACKAGE_NAME + ".DLC.STUDY";
    public static final String ACTION_VERIFY_CONTENT = AppConstants.ANDROID_PACKAGE_NAME + ".DLC.VERIFY";
    public static final String ACTION_DOWNLOAD_CONTENT = AppConstants.ANDROID_PACKAGE_NAME + ".DLC.DOWNLOAD";

    private static final String CDN_BASE_URL = "https://mobile.cdn.mozilla.net/";

    private static final String CACHE_DIRECTORY = "downloadContent";

    /**
     * Exception indicating a recoverable error has happened. Download of the content will be retried later.
     */
    /* package-private */ static class RecoverableDownloadContentException extends Exception {
        private static final long serialVersionUID = -2246772819507370734L;

        public RecoverableDownloadContentException(String message) {
            super(message);
        }

        public RecoverableDownloadContentException(Throwable cause) {
            super(cause);
        }
    }

    /**
     * If this exception is thrown the content will be marked as unrecoverable, permanently failed and we will not try
     * downloading it again - until a newer version of the content is available.
     */
    /* package-private */ static class UnrecoverableDownloadContentException extends Exception {
        private static final long serialVersionUID = 8956080754787367105L;

        public UnrecoverableDownloadContentException(String message) {
            super(message);
        }

        public UnrecoverableDownloadContentException(Throwable cause) {
            super(cause);
        }
    }

    /* package-private */ static HttpClient buildHttpClient() {
        // TODO: Implement proxy support (Bug 1209496)
        return HttpClientBuilder.create()
            .setUserAgent(HardwareUtils.isTablet() ?
                    AppConstants.USER_AGENT_FENNEC_TABLET :
                    AppConstants.USER_AGENT_FENNEC_MOBILE)
            .setRetryHandler(new DefaultHttpRequestRetryHandler())
            .build();
    }

    /* package-private */ static File createTemporaryFile(Context context, DownloadContent content)
            throws RecoverableDownloadContentException {
        File cacheDirectory = new File(context.getCacheDir(), CACHE_DIRECTORY);

        if (!cacheDirectory.exists() && !cacheDirectory.mkdirs()) {
            // Recoverable: File system might not be mounted NOW and we didn't download anything yet anyways.
            throw new RecoverableDownloadContentException("Could not create cache directory: " + cacheDirectory);
        }

        return new File(cacheDirectory, content.getDownloadChecksum() + "-" + content.getId());
    }

    /* package-private */ static void download(HttpClient client, String source, File temporaryFile)
            throws RecoverableDownloadContentException, UnrecoverableDownloadContentException {
        InputStream inputStream = null;
        OutputStream outputStream = null;

        final HttpGet request = new HttpGet(source);

        try {
            final HttpResponse response = client.execute(request);
            final int status = response.getStatusLine().getStatusCode();
            if (status != HttpStatus.SC_OK) {
                // We are trying to be smart and only retry if this is an error that might resolve in the future.
                // TODO: This is guesstimating at best. We want to implement failure counters (Bug 1215106).
                if (status >= 500) {
                    // Recoverable: Server errors 5xx
                    throw new RecoverableDownloadContentException("(Recoverable) Download failed. Status code: " + status);
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
                throw new RecoverableDownloadContentException("Null entity");
            }

            inputStream = new BufferedInputStream(entity.getContent());
            outputStream = new BufferedOutputStream(new FileOutputStream(temporaryFile));

            IOUtils.copy(inputStream, outputStream);

            inputStream.close();
            outputStream.close();
        } catch (IOException e) {
            // TODO: Support resuming downloads using 'Range' header (Bug 1209513)
            temporaryFile.delete();

            // Recoverable: Just I/O discontinuation
            throw new RecoverableDownloadContentException(e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
            IOUtils.safeStreamClose(outputStream);
        }
    }

    /* package-private */ static boolean verify(File file, String expectedChecksum)
            throws RecoverableDownloadContentException, UnrecoverableDownloadContentException {
        InputStream inputStream = null;

        try {
            inputStream = new BufferedInputStream(new FileInputStream(file));

            byte[] ctx = NativeCrypto.sha256init();
            if (ctx == null) {
                throw new RecoverableDownloadContentException("Could not create SHA-256 context");
            }

            byte[] buffer = new byte[4096];
            int read;

            while ((read = inputStream.read(buffer)) != -1) {
                NativeCrypto.sha256update(ctx, buffer, read);
            }

            String actualChecksum = Utils.byte2Hex(NativeCrypto.sha256finalize(ctx));

            if (!actualChecksum.equalsIgnoreCase(expectedChecksum)) {
                Log.w(LOGTAG, "Checksums do not match. Expected=" + expectedChecksum + ", Actual=" + actualChecksum);
                return false;
            }

            return true;
        } catch (IOException e) {
            // Recoverable: Just I/O discontinuation
            throw new RecoverableDownloadContentException(e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
        }
    }

    /* package-private */ static void move(File temporaryFile, File destinationFile)
            throws RecoverableDownloadContentException, UnrecoverableDownloadContentException {
        if (!temporaryFile.renameTo(destinationFile)) {
            Log.d(LOGTAG, "Could not move temporary file to destination. Trying to copy..");
            copy(temporaryFile, destinationFile);
            temporaryFile.delete();
        }
    }

    /* package-private */ static void extract(File sourceFile, File destinationFile, String checksum)
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
            // We do not support resume yet (Bug 1209513). Therefore we have to treat this as unrecoverable: The
            // temporarily file will be deleted and we want to avoid downloading and failing repeatedly.
            throw new UnrecoverableDownloadContentException(e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
            IOUtils.safeStreamClose(outputStream);

            if (temporaryFile != null && temporaryFile.exists()) {
                temporaryFile.delete();
            }
        }
    }

    private static void copy(File temporaryFile, File destinationFile)
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
            // Meh. This is an awkward situation: We downloaded the content but we can't move it to its destination. We
            // are treating this as "unrecoverable" error because we want to avoid downloading this again and again and
            // then always failing to copy it to the destination. This will be fixed after we implement resuming
            // downloads (Bug 1209513): We will keep the downloaded temporary file and just retry copying.
            throw new UnrecoverableDownloadContentException(e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
            IOUtils.safeStreamClose(outputStream);
        }
    }

    /* package-private */ static File getDestinationFile(Context context, DownloadContent content)  throws UnrecoverableDownloadContentException {
        if (content.isFont()) {
            return new File(new File(context.getApplicationInfo().dataDir, "fonts"), content.getFilename());
        }

        // Unrecoverable: We downloaded a file and we don't know what to do with it (Should not happen)
        throw new UnrecoverableDownloadContentException("Can't determine destination for kind: " + content.getKind());
    }

    /* package-private */ static boolean isActiveNetworkMetered(Context context) {
        return ConnectivityManagerCompat.isActiveNetworkMetered(
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE));
    }

    /* package-private */ static String createDownloadURL(DownloadContent content) {
        return CDN_BASE_URL + content.getLocation();
    }
}
