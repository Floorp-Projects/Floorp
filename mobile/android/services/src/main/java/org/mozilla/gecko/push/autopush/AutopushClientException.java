/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push.autopush;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpStatus;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

public class AutopushClientException extends Exception {
    private static final long serialVersionUID = 7953459541558266500L;

    public AutopushClientException(String detailMessage) {
        super(detailMessage);
    }

    public AutopushClientException(Exception e) {
        super(e);
    }

    public boolean isTransientError() {
        return false;
    }

    public static class AutopushClientRemoteException extends AutopushClientException {
        private static final long serialVersionUID = 2209313149952001000L;

        public final HttpResponse response;
        public final long httpStatusCode;
        public final long apiErrorNumber;
        public final String error;
        public final String message;
        public final ExtendedJSONObject body;

        public AutopushClientRemoteException(HttpResponse response, long httpStatusCode, long apiErrorNumber, String error, String message, ExtendedJSONObject body) {
            super(new HTTPFailureException(new SyncStorageResponse(response)));
            if (body == null) {
                throw new IllegalArgumentException("body must not be null");
            }
            this.response = response;
            this.httpStatusCode = httpStatusCode;
            this.apiErrorNumber = apiErrorNumber;
            this.error = error;
            this.message = message;
            this.body = body;
        }

        @Override
        public String toString() {
            return "<AutopushClientRemoteException " + this.httpStatusCode + " [" + this.apiErrorNumber + "]: " + this.message + ">";
        }

        public boolean isInvalidAuthentication() {
            return httpStatusCode == HttpStatus.SC_UNAUTHORIZED;
        }

        public boolean isNotFound() {
            return httpStatusCode == HttpStatus.SC_NOT_FOUND;
        }

        public boolean isGone() {
            return httpStatusCode == HttpStatus.SC_GONE;
        }

        @Override
        public boolean isTransientError() {
            return httpStatusCode >= 500;
        }
    }

    public static class AutopushClientMalformedResponseException extends AutopushClientRemoteException {
        private static final long serialVersionUID = 2209313149952001909L;

        public AutopushClientMalformedResponseException(HttpResponse response) {
            super(response, 0, 999, "Response malformed", "Response malformed", new ExtendedJSONObject());
        }
    }
}
