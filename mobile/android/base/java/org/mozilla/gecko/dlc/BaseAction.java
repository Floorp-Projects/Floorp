/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.annotation.SuppressLint;
import android.content.Context;
import android.support.annotation.IntDef;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.background.nativecode.NativeCrypto;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.ProxySelector;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URISyntaxException;

public abstract class BaseAction {
    private static final String LOGTAG = "GeckoDLCBaseAction";

    /**
     * Exception indicating a recoverable error has happened. Download of the content will be retried later.
     */
    /* package-private */ static class RecoverableDownloadContentException extends Exception {
        private static final long serialVersionUID = -2246772819507370734L;

        @IntDef({MEMORY, DISK_IO, SERVER, NETWORK})
        public @interface ErrorType {}
        public static final int MEMORY = 1;
        public static final int DISK_IO = 2;
        public static final int SERVER = 3;
        public static final int NETWORK = 4;

        private int errorType;

        public RecoverableDownloadContentException(@ErrorType int errorType, String message) {
            super(message);
            this.errorType = errorType;
        }

        public RecoverableDownloadContentException(@ErrorType int errorType, Throwable cause) {
            super(cause);
            this.errorType = errorType;
        }

        @ErrorType
        public int getErrorType() {
            return errorType;
        }

        /**
         * Should this error be counted as failure? If this type of error will happen multiple times in a row then this
         * error will be treated as permanently and the operation will not be tried again until the content changes.
         */
        public boolean shouldBeCountedAsFailure() {
            if (NETWORK == errorType) {
                return false; // Always retry after network errors
            }

            return true;
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

    public abstract void perform(Context context, DownloadContentCatalog catalog);

    protected File getDestinationFile(Context context, DownloadContent content)
            throws UnrecoverableDownloadContentException, RecoverableDownloadContentException {
        File destinationDirectory;
        if (content.isFont()) {
            destinationDirectory = new File(context.getApplicationInfo().dataDir, "fonts");
        } else if (content.isHyphenationDictionary()) {
            destinationDirectory = new File(context.getApplicationInfo().dataDir, "hyphenation");
        } else {
            throw new UnrecoverableDownloadContentException("Can't determine destination for kind: " + (String) content.getKind());
        }

        if (!destinationDirectory.exists() && !destinationDirectory.mkdirs()) {
            throw new RecoverableDownloadContentException(RecoverableDownloadContentException.DISK_IO,
                    "Destination directory does not exist and cannot be created");
        }

        return new File(destinationDirectory, content.getFilename());
    }

    protected boolean verify(File file, String expectedChecksum)
            throws RecoverableDownloadContentException, UnrecoverableDownloadContentException {
        InputStream inputStream = null;

        try {
            inputStream = new BufferedInputStream(new FileInputStream(file));

            byte[] ctx = NativeCrypto.sha256init();
            if (ctx == null) {
                throw new RecoverableDownloadContentException(RecoverableDownloadContentException.MEMORY,
                                                              "Could not create SHA-256 context");
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
            throw new RecoverableDownloadContentException(RecoverableDownloadContentException.DISK_IO, e);
        } finally {
            IOUtils.safeStreamClose(inputStream);
        }
    }

    protected HttpURLConnection buildHttpURLConnection(String url)
            throws UnrecoverableDownloadContentException, IOException {
        try {
            System.setProperty("http.keepAlive", "true");

            HttpURLConnection connection = (HttpURLConnection) ProxySelector.openConnectionWithProxy(new URI(url));
            connection.setRequestProperty("User-Agent", HardwareUtils.isTablet() ?
                    AppConstants.USER_AGENT_FENNEC_TABLET :
                    AppConstants.USER_AGENT_FENNEC_MOBILE);
            connection.setRequestMethod("GET");
            connection.setInstanceFollowRedirects(true);
            return connection;
        } catch (MalformedURLException e) {
            throw new UnrecoverableDownloadContentException(e);
        } catch (URISyntaxException e) {
            throw new UnrecoverableDownloadContentException(e);
        }
    }
}
