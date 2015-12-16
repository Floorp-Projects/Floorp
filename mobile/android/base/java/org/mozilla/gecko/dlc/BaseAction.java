/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;
import android.util.Log;

import org.mozilla.gecko.background.nativecode.NativeCrypto;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.IOUtils;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

public abstract class BaseAction {
    private static final String LOGTAG = "GeckoDLCBaseAction";

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

    public abstract void perform(Context context, DownloadContentCatalog catalog);

    protected File getDestinationFile(Context context, DownloadContent content) throws UnrecoverableDownloadContentException {
        if (content.isFont()) {
            return new File(new File(context.getApplicationInfo().dataDir, "fonts"), content.getFilename());
        }

        // Unrecoverable: We downloaded a file and we don't know what to do with it (Should not happen)
        throw new UnrecoverableDownloadContentException("Can't determine destination for kind: " + content.getKind());
    }

    protected boolean verify(File file, String expectedChecksum)
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
}
