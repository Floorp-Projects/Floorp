/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.permissions;

import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.Context;
import android.support.annotation.NonNull;

/**
 * Helper class to run code blocks depending on whether a user has granted or denied certain runtime permissions.
 */
public class PermissionBlock {
    private final PermissionsHelper helper;

    private Activity activity;
    private String[] permissions;
    private boolean onUIThread;
    private Runnable onPermissionsGranted;
    private Runnable onPermissionsDenied;

    /* package-private */ PermissionBlock(Activity activity, PermissionsHelper helper) {
        this.activity = activity;
        this.helper = helper;
    }

    /**
     * Determine whether the app has been granted the specified permissions.
     */
    public PermissionBlock withPermissions(@NonNull String... permissions) {
        this.permissions = permissions;
        return this;
    }

    /**
     * Execute all callbacks on the UI thread.
     */
    public PermissionBlock onUIThread() {
        this.onUIThread = true;
        return this;
    }

    /**
     * Execute the specified runnable if the app has been granted all permissions. Calling this method will prompt the
     * user if needed.
     */
    public void run(@NonNull Runnable onPermissionsGranted) {
        this.onPermissionsGranted = onPermissionsGranted;

        if (hasPermissions(activity)) {
            onPermissionsGranted();
        } else {
            Permissions.prompt(activity, this);
        }

        // This reference is no longer needed. Let's clear it now to avoid memory leaks.
        activity = null;
    }

    /**
     * Execute this fallback if at least one permission has not been granted.
     */
    public PermissionBlock andFallback(@NonNull Runnable onPermissionsDenied) {
        this.onPermissionsDenied = onPermissionsDenied;
        return this;
    }

    /* package-private */ void onPermissionsGranted() {
        executeRunnable(onPermissionsGranted);
    }

    /* package-private */ void onPermissionsDenied() {
        executeRunnable(onPermissionsDenied);
    }

    private void executeRunnable(Runnable runnable) {
        if (runnable == null) {
            return;
        }

        if (onUIThread) {
            ThreadUtils.postToUiThread(runnable);
        } else {
            runnable.run();
        }
    }

    /* package-private */ String[] getPermissions() {
        return permissions;
    }

    /* packacge-private */ boolean hasPermissions(Context context) {
        return helper.hasPermissions(context, permissions);
    }
}
