/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.annotation.WrapForJNI;

import android.annotation.SuppressLint;
import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import java.io.ByteArrayInputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;

/**
 * WebRequestError is simply a container for error codes and categories used by
 * {@link GeckoSession.NavigationDelegate#onLoadError(GeckoSession, String, WebRequestError)}.
 */
@AnyThread
public class WebRequestError extends Exception {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ERROR_CATEGORY_UNKNOWN, ERROR_CATEGORY_SECURITY,
            ERROR_CATEGORY_NETWORK, ERROR_CATEGORY_CONTENT,
            ERROR_CATEGORY_URI, ERROR_CATEGORY_PROXY,
            ERROR_CATEGORY_SAFEBROWSING})
    /* package */ @interface ErrorCategory {}

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ERROR_UNKNOWN, ERROR_SECURITY_SSL, ERROR_SECURITY_BAD_CERT,
            ERROR_NET_RESET, ERROR_NET_INTERRUPT, ERROR_NET_TIMEOUT,
            ERROR_CONNECTION_REFUSED, ERROR_UNKNOWN_PROTOCOL,
            ERROR_UNKNOWN_HOST, ERROR_UNKNOWN_SOCKET_TYPE,
            ERROR_UNKNOWN_PROXY_HOST, ERROR_MALFORMED_URI,
            ERROR_REDIRECT_LOOP, ERROR_SAFEBROWSING_PHISHING_URI,
            ERROR_SAFEBROWSING_MALWARE_URI, ERROR_SAFEBROWSING_UNWANTED_URI,
            ERROR_SAFEBROWSING_HARMFUL_URI, ERROR_CONTENT_CRASHED,
            ERROR_OFFLINE, ERROR_PORT_BLOCKED,
            ERROR_PROXY_CONNECTION_REFUSED, ERROR_FILE_NOT_FOUND,
            ERROR_FILE_ACCESS_DENIED, ERROR_INVALID_CONTENT_ENCODING,
            ERROR_UNSAFE_CONTENT_TYPE, ERROR_CORRUPTED_CONTENT,
            ERROR_DATA_URI_TOO_LONG})
    /* package */ @interface Error {}

    /**
     * This is normally used for error codes that don't
     * currently fit into any of the other categories.
     */
    public static final int ERROR_CATEGORY_UNKNOWN = 0x1;

    /**
     * This is used for error codes that relate to SSL
     * certificate validation.
     */
    public static final int ERROR_CATEGORY_SECURITY = 0x2;

    /**
     * This is used for error codes relating to network
     * problems.
     */
    public static final int ERROR_CATEGORY_NETWORK = 0x3;

    /**
     * This is used for error codes relating to invalid
     * or corrupt web pages.
     */
    public static final int ERROR_CATEGORY_CONTENT = 0x4;
    public static final int ERROR_CATEGORY_URI = 0x5;
    public static final int ERROR_CATEGORY_PROXY = 0x6;
    public static final int ERROR_CATEGORY_SAFEBROWSING = 0x7;

    /**
     * An unknown error occurred
     */
    public static final int ERROR_UNKNOWN = 0x11;

    // Security
    /**
     * This is used for a variety of SSL negotiation problems.
     */
    public static final int ERROR_SECURITY_SSL = 0x22;

    /**
     * This is used to indicate an untrusted or otherwise
     * invalid SSL certificate.
     */
    public static final int ERROR_SECURITY_BAD_CERT = 0x32;

    // Network
    /**
     * The network connection was interrupted.
     */
    public static final int ERROR_NET_INTERRUPT = 0x23;

    /**
     * The network request timed out.
     */
    public static final int ERROR_NET_TIMEOUT = 0x33;

    /**
     * The network request was refused by the server.
     */
    public static final int ERROR_CONNECTION_REFUSED = 0x43;

    /**
     * The network request tried to use an unknown socket type.
     */
    public static final int ERROR_UNKNOWN_SOCKET_TYPE = 0x53;

    /**
     * A redirect loop was detected.
     */
    public static final int ERROR_REDIRECT_LOOP = 0x63;

    /**
     * This device does not have a network connection.
     */
    public static final int ERROR_OFFLINE = 0x73;

    /**
     * The request tried to use a port that is blocked by either the OS or Gecko.
     */
    public static final int ERROR_PORT_BLOCKED = 0x83;

    /**
     * The connection was reset.
     */
    public static final int ERROR_NET_RESET = 0x93;

    // Content
    /**
     * A content type was returned which was deemed unsafe.
     */
    public static final int ERROR_UNSAFE_CONTENT_TYPE = 0x24;

    /**
     * The content returned was corrupted.
     */
    public static final int ERROR_CORRUPTED_CONTENT = 0x34;

    /**
     * The content process crashed.
     */
    public static final int ERROR_CONTENT_CRASHED = 0x44;

    /**
     * The content has an invalid encoding.
     */
    public static final int ERROR_INVALID_CONTENT_ENCODING = 0x54;

    // URI
    /**
     * The host could not be resolved.
     */
    public static final int ERROR_UNKNOWN_HOST = 0x25;

    /**
     * An invalid URL was specified.
     */
    public static final int ERROR_MALFORMED_URI = 0x35;

    /**
     * An unknown protocol was specified.
     */
    public static final int ERROR_UNKNOWN_PROTOCOL = 0x45;

    /**
     * A file was not found (usually used for file:// URIs).
     */
    public static final int ERROR_FILE_NOT_FOUND = 0x55;

    /**
     * The OS blocked access to a file.
     */
    public static final int ERROR_FILE_ACCESS_DENIED = 0x65;

    /**
     * A data:// URI is too long to load at the top level.
     */
    public static final int ERROR_DATA_URI_TOO_LONG = 0x75;

    // Proxy
    /**
     * The proxy server refused the connection.
     */
    public static final int ERROR_PROXY_CONNECTION_REFUSED = 0x26;

    /**
     * The host name of the proxy server could not be resolved.
     */
    public static final int ERROR_UNKNOWN_PROXY_HOST = 0x36;

    // Safebrowsing
    /**
     * The requested URI was present in the "malware" blocklist.
     */
    public static final int ERROR_SAFEBROWSING_MALWARE_URI = 0x27;

    /**
     * The requested URI was present in the "unwanted" blocklist.
     */
    public static final int ERROR_SAFEBROWSING_UNWANTED_URI = 0x37;

    /**
     * The requested URI was present in the "harmful" blocklist.
     */
    public static final int ERROR_SAFEBROWSING_HARMFUL_URI = 0x47;

    /**
     * The requested URI was present in the "phishing" blocklist.
     */
    public static final int ERROR_SAFEBROWSING_PHISHING_URI = 0x57;

    /**
     * The error code, e.g. {@link #ERROR_MALFORMED_URI}.
     */
    public final int code;

    /**
     * The error category, e.g. {@link #ERROR_CATEGORY_URI}.
     */
    public final int category;

    /**
     * The server certificate used. This can be useful if the error code is
     * is e.g. {@link #ERROR_SECURITY_BAD_CERT}.
     */
    public final @Nullable X509Certificate certificate;

    /**
     * Construct a new WebRequestError with the specified code and category.
     * @param code An error code, e.g. {@link #ERROR_MALFORMED_URI}
     * @param category An error category, e.g. {@link #ERROR_CATEGORY_URI}
     */
    public WebRequestError(final @Error int code, final @ErrorCategory int category) {
        this(code, category, null);
    }

    /**
     * Construct a new WebRequestError with the specified code and category.
     * @param code An error code, e.g. {@link #ERROR_MALFORMED_URI}
     * @param category An error category, e.g. {@link #ERROR_CATEGORY_URI}
     * @param certificate The X509Certificate server certificate used, if applicable.
     */
    public WebRequestError(final @Error int code, final @ErrorCategory int category, final X509Certificate certificate) {
        super(String.format("Request failed, error=0x%x, category=0x%x", code, category));
        this.code = code;
        this.category = category;
        this.certificate = certificate;
    }

    @Override
    public boolean equals(final Object other) {
        if (other == null || !(other instanceof WebRequestError)) {
            return false;
        }

        final WebRequestError otherError = (WebRequestError)other;

        // We don't compare the certificate here because it's almost never what you want.
        return otherError.code == this.code &&
                otherError.category == this.category;
    }

    @Override
    public int hashCode() {
        return (category << 16) + code;
    }

    @WrapForJNI
    /* package */ static WebRequestError fromGeckoError(final long geckoError,
                                                        final int geckoErrorModule,
                                                        final int geckoErrorClass,
                                                        final byte[] certificateBytes) {
        final int code = convertGeckoError(geckoError, geckoErrorModule, geckoErrorClass);
        final int category = getErrorCategory(geckoErrorModule, code);
        X509Certificate certificate = null;
        if (certificateBytes != null) {
            try {
                final CertificateFactory factory = CertificateFactory.getInstance("X.509");
                certificate = (X509Certificate) factory.generateCertificate(new ByteArrayInputStream(certificateBytes));
            } catch (final CertificateException e) {
                throw new IllegalArgumentException("Unable to parse DER certificate");
            }
        }

        return new WebRequestError(code, category, certificate);
    }

    @SuppressLint("WrongConstant")
    @WrapForJNI
    /* package */ static @ErrorCategory int getErrorCategory(
            final long errorModule, final @Error int error) {
        // Match flags with XPCOM ErrorList.h.
        if (errorModule == 21) {
            return ERROR_CATEGORY_SECURITY;
        }
        return error & 0xF;
    }

    @WrapForJNI
    /* package */ static @Error int convertGeckoError(
            final long geckoError, final int geckoErrorModule, final int geckoErrorClass) {
        // Match flags with XPCOM ErrorList.h.
        // safebrowsing
        if (geckoError == 0x805D001FL) {
            return ERROR_SAFEBROWSING_PHISHING_URI;
        }
        if (geckoError == 0x805D001EL) {
            return ERROR_SAFEBROWSING_MALWARE_URI;
        }
        if (geckoError == 0x805D0023L) {
            return ERROR_SAFEBROWSING_UNWANTED_URI;
        }
        if (geckoError == 0x805D0026L) {
            return ERROR_SAFEBROWSING_HARMFUL_URI;
        }
        // content
        if (geckoError == 0x805E0010L) {
            return ERROR_CONTENT_CRASHED;
        }
        if (geckoError == 0x804B001BL) {
            return ERROR_INVALID_CONTENT_ENCODING;
        }
        if (geckoError == 0x804B004AL) {
            return ERROR_UNSAFE_CONTENT_TYPE;
        }
        if (geckoError == 0x804B001DL) {
            return ERROR_CORRUPTED_CONTENT;
        }
        // network
        if (geckoError == 0x804B0014L) {
            return ERROR_NET_RESET;
        }
        if (geckoError == 0x804B0047L) {
            return ERROR_NET_INTERRUPT;
        }
        if (geckoError == 0x804B000EL) {
            return ERROR_NET_TIMEOUT;
        }
        if (geckoError == 0x804B000DL) {
            return ERROR_CONNECTION_REFUSED;
        }
        if (geckoError == 0x804B0033L) {
            return ERROR_UNKNOWN_SOCKET_TYPE;
        }
        if (geckoError == 0x804B001FL) {
            return ERROR_REDIRECT_LOOP;
        }
        if (geckoError == 0x804B0010L) {
            return ERROR_OFFLINE;
        }
        if (geckoError == 0x804B0013L) {
            return ERROR_PORT_BLOCKED;
        }
        // uri
        if (geckoError == 0x804B0012L) {
            return ERROR_UNKNOWN_PROTOCOL;
        }
        if (geckoError == 0x804B001EL) {
            return ERROR_UNKNOWN_HOST;
        }
        if (geckoError == 0x804B000AL) {
            return ERROR_MALFORMED_URI;
        }
        if (geckoError == 0x80520012L) {
            return ERROR_FILE_NOT_FOUND;
        }
        if (geckoError == 0x80520015L) {
            return ERROR_FILE_ACCESS_DENIED;
        }
        // proxy
        if (geckoError == 0x804B002AL) {
            return ERROR_UNKNOWN_PROXY_HOST;
        }
        if (geckoError == 0x804B0048L) {
            return ERROR_PROXY_CONNECTION_REFUSED;
        }

        if (geckoErrorModule == 21) {
            if (geckoErrorClass == 1) {
                return ERROR_SECURITY_SSL;
            }
            if (geckoErrorClass == 2) {
                return ERROR_SECURITY_BAD_CERT;
            }
        }

        return ERROR_UNKNOWN;
    }
}
