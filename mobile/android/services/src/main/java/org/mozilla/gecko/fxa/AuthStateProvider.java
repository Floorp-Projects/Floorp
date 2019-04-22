/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import android.annotation.TargetApi;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import org.mozilla.apache.commons.codec.digest.DigestUtils;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Married;
import org.mozilla.gecko.fxa.login.State;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Provides external access to Firefox Account state. This allows sharing account state with other
 * applications. Access is provided based on a signature whitelist.
 *
 * Consumers of this must verify signature of the applicationId which provides a ContentProvider
 * servicing AUTHORITY. Failure to do so may lead to interacting with a ContentProvider that's squatting
 * our AUTHORITY.
 */
public class AuthStateProvider extends ContentProvider {
    static final String LOG_TAG = "AuthStateProvider";
    static final String AUTHORITY = AppConstants.ANDROID_PACKAGE_NAME + ".fxa.auth";
    static final String AUTH_STATE_CONTENT_TYPE = "vnd.android.cursor.item/vnd." + AUTHORITY + ".state";
    static final int AUTH_STATE = 100;

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static final String KEY_EMAIL = "email";
    static final String KEY_SESSION_TOKEN = "sessionToken";
    static final String KEY_KSYNC = "kSync";
    static final String KEY_KXSCS = "kXSCS";

    private final String[] queryProjection = new String[] { KEY_EMAIL, KEY_SESSION_TOKEN, KEY_KSYNC, KEY_KXSCS };

    static {
        URI_MATCHER.addURI(AUTHORITY, "state", AUTH_STATE);
    }

    @Override
    public boolean onCreate() {
        // We could pre-fetch the account state, but that won't win us very much, and will make state
        // management more complicated as things become potentially stale. So, this is a no-op.
        return true;
    }

    @Nullable
    @Override
    public Cursor query(@NonNull Uri uri, @Nullable String[] projection, @Nullable String selection, @Nullable String[] selectionArgs, @Nullable String sortOrder) {
        final Context context = getContext();
        if (context == null) {
            // This indicated that onCreate didn't run yet, which is quite strange.
            return null;
        }
        if (!isTrustedCaller(context)) {
            Log.d(LOG_TAG, "Caller must be whitelisted.");
            return null;
        }

        final int match = URI_MATCHER.match(uri);

        switch (match) {
            case AUTH_STATE: {
                @Nullable final AndroidFxAccount fxaAccount = AndroidFxAccount.fromContext(getContext());
                // No account.
                if (fxaAccount == null) {
                    return null;
                }

                final String email = fxaAccount.getEmail();

                // We may fail to read our account's state due to a number of internal issues.
                // Be defensive.
                final State accountState;
                try {
                    accountState = fxaAccount.getState();
                } catch (Exception e) {
                    Log.e(LOG_TAG, "Failed to read account state", e);
                    return null;
                }

                // Cursor with an initial capacity of 1. We just have a single row to put into it.
                final MatrixCursor cursor = new MatrixCursor(queryProjection, 1);

                // We have an account, but it may not be in a fully-functioning state.
                // If we're not "Married" (which is the final, "all good" state), only return an 'email'.
                // Otherwise, return the necessary token/keys.
                if (accountState instanceof Married) {
                    final Married marriedState = (Married) accountState;
                    final byte[] sessionToken = marriedState.getSessionToken();
                    final byte[] kSync = marriedState.getKSync();
                    final String kXSCS = marriedState.getKXCS();

                    cursor.addRow(new Object[] { email, sessionToken, kSync, kXSCS });
                } else {
                    cursor.addRow(new Object[] { email, null, null, null });
                }

                return cursor;
            }
            default: {
                // Don't throw since we don't want to blow up the world, but at least log an error.
                Log.e(LOG_TAG, "Unknown query URI " + uri);
            }
        }
        return null;
    }

    @Nullable
    @Override
    public String getType(@NonNull Uri uri) {
        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case AUTH_STATE: return AUTH_STATE_CONTENT_TYPE;
        }
        return null;
    }

    // We explicitly do not support any 'edit' operations; throw if we see them!

    @Nullable
    @Override
    public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
        throw new UnsupportedOperationException("Insert operation not supported");
    }

    @Override
    public int delete(@NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
        throw new UnsupportedOperationException("Delete operation not supported");
    }

    @Override
    public int update(@NonNull Uri uri, @Nullable ContentValues values, @Nullable String selection, @Nullable String[] selectionArgs) {
        throw new UnsupportedOperationException("Update operation not supported");
    }

    /**
     * A trusted caller must appear exactly in our whitelist map - its package map must map to a known signature.
     * In case of any deviation (multiple signers, certificate rotation), assume that the caller isn't trusted.
     *
     * @param context Application context necessary for obtaining an instance of the PackageManager.
     * @return a boolean flag indicating whether or not our caller can be trusted.
     */
    private boolean isTrustedCaller(@NonNull Context context) {
        final PackageManager packageManager = context.getPackageManager();

        // We will only service query requests from callers that exactly match our whitelist.
        // Whitelist is local to this function to avoid exposing it to the world more than necessary.
        final HashMap<String, String> packageToSignatureWhitelist = new HashMap<>();
        // Main Fenix channel.
        packageToSignatureWhitelist.put(
            "org.mozilla.fenix", "5004779088e7f988d5bc5cc5f8798febf4f8cd084a1b2a46efd4c8ee4aeaf211"
        );
        // Main Lockwise channel.
        packageToSignatureWhitelist.put(
            "mozilla.lockbox", "64d26b507078deba2fee42d6bd0bfad41d39ffc4e791f281028e5e73d3c8d2f2"
        );
        // We'll operate over an immutable version of the package whitelist.
        // This could be unnecessarily paranoid, but won't hurt. We can eat the performance penalty.
        final Map<String, String> immutablePackageWhitelist = Collections.unmodifiableMap(
            new HashMap<String, String>(packageToSignatureWhitelist)
        );

        // If we can't read caller's package name, bail out.
        final String callerPackage = getCallerPackage(packageManager);
        if (callerPackage == null) {
            return false;
        }
        // If we don't have caller's package name in our whitelist map, bail out.
        final String expectedHash = immutablePackageWhitelist.get(callerPackage);
        if (expectedHash == null) {
            return false;
        }

        final Signature callerSignature;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            callerSignature = getSignaturePostAPI28(packageManager, callerPackage);
        } else {
            callerSignature = getSignaturePreAPI28(packageManager, callerPackage);
        }

        // If we couldn't obtain caller's signature, bail out.
        if (callerSignature == null) {
            return false;
        }

        // Make sure caller's signature hash matches what's in our whitelist.
        final String callerSignatureHash = DigestUtils.sha256Hex(callerSignature.toByteArray());
        Log.d(LOG_TAG, "Verifying caller's signature:" + callerSignatureHash);
        final boolean result = expectedHash.equals(callerSignatureHash);
        if (result) {
            Log.d(LOG_TAG, "Success!");
        } else {
            Log.d(LOG_TAG, "Failed! Signature mismatch for calling package " + callerPackage);
        }
        return result;
    }

    @Nullable
    private static Signature getSignaturePreAPI28(final PackageManager packageManager, final String callerPackage) {
        // For older APIs, we use the deprecated `signatures` field, which isn't aware of certificate rotation.
        final PackageInfo packageInfo;
        try {
            packageInfo = packageManager.getPackageInfo(callerPackage, PackageManager.GET_SIGNATURES);
        } catch (PackageManager.NameNotFoundException e) {
            throw new IllegalStateException("Package name no longer present");
        }
        // We don't expect our callers to have multiple signers, so we don't service such requests.
        if (packageInfo.signatures.length != 1) {
            return null;
        }
        // In case of signature rotation, this will report the oldest used certificate,
        // pretending that the signature rotation never took place.
        // We can only rely on our whitelist being up-to-date in this case.
        return packageInfo.signatures[0];
    }

    @TargetApi(Build.VERSION_CODES.P)
    @Nullable
    private Signature getSignaturePostAPI28(final PackageManager packageManager, final String callerPackage) {
        // For API28+, we can perform some extra checks.
        final PackageInfo packageInfo;
        try {
            packageInfo = packageManager.getPackageInfo(callerPackage, PackageManager.GET_SIGNING_CERTIFICATES);
        } catch (PackageManager.NameNotFoundException e) {
            throw new IllegalStateException("Package name no longer present");
        }
        // We don't expect our callers to have multiple signers, so we don't service such requests.
        if (packageInfo.signingInfo.hasMultipleSigners()) {
            return null;
        }
        // We currently don't support servicing requests from callers that performed certificate rotation.
        if (packageInfo.signingInfo.hasPastSigningCertificates()) {
            return null;
        }
        return packageInfo.signingInfo.getSigningCertificateHistory()[0];
    }

    @Nullable
    private String getCallerPackage(PackageManager packageManager) {
        final int callerUid = Binder.getCallingUid();
        // We can always obtain our calling package via `uid` directly.
        final String legacyCallingPackage = packageManager.getNameForUid(callerUid);

        if (legacyCallingPackage == null) {
            return null;
        }

        // On API19+, we can ask ContentProvider (ourselves) for the calling package.
        // Double-check what we've obtained manually, bail out in case of disagreements.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            final String callingPackage = getCallingPackage();

            if (callingPackage == null) {
                Log.e(LOG_TAG, "Manually obtained a calling package, but got null from ContentProvider.getCallingPackage()");
                return null;
            }

            if (!legacyCallingPackage.equals(callingPackage)) {
                Log.e(LOG_TAG, "Calling package disagreement. Legacy: " + legacyCallingPackage + ", new API: " + callingPackage);
                return null;
            }
        }

        return legacyCallingPackage;
    }
}
