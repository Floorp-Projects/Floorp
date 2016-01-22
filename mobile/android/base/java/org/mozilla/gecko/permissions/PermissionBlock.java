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

    private Context context;
    private String[] permissions;
    private boolean onUIThread;
    private Runnable onPermissionsGranted;
    private Runnable onPermissionsDenied;
    private boolean doNotPrompt;

    /* package-private */ PermissionBlock(Context context, PermissionsHelper helper) {
        this.context = context;
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
     * Do not prompt the user to accept the permission if it has not been granted yet.
     */
    public PermissionBlock doNotPrompt() {
        doNotPrompt = true;
        return this;
    }

    /**
     * If the condition is true then do not prompt the user to accept the permission if it has not
     * been granted yet.
     */
    public PermissionBlock doNotPromptIf(boolean condition) {
        if (condition) {
            doNotPrompt();
        }

        return this;
    }

    /**
     * Execute this permission block. Calling this method will prompt the user if needed.
     */
    public void run() {
        run(null);
    }

    /**
     * Execute the specified runnable if the app has been granted all permissions. Calling this method will prompt the
     * user if needed.
     */
    public void run(Runnable onPermissionsGranted) {
        if (!doNotPrompt && !(context instanceof Activity)) {
            throw new IllegalStateException("You need to either specify doNotPrompt() or pass in an Activity context");
        }

        this.onPermissionsGranted = onPermissionsGranted;

        if (hasPermissions(context)) {
            onPermissionsGranted();
        } else if (doNotPrompt) {
            onPermissionsDenied();
        } else {
            Permissions.prompt((Activity) context, this);
        }

        // This reference is no longer needed. Let's clear it now to avoid memory leaks.
        context = null;
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
