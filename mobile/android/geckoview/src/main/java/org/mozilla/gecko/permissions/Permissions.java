/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.permissions;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.support.annotation.NonNull;

import org.mozilla.gecko.util.ThreadUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.Callable;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/**
 * Convenience class for checking and prompting for runtime permissions.
 *
 * Example:
 *
 *    Permissions.from(activity)
 *               .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
 *               .onUiThread()
 *               .andFallback(onPermissionDenied())
 *               .run(onPermissionGranted())
 *
 * This example will run the runnable returned by onPermissionGranted() if the WRITE_EXTERNAL_STORAGE permission is
 * already granted. Otherwise it will prompt the user and run the runnable returned by onPermissionGranted() or
 * onPermissionDenied() depending on whether the user accepted or not. If onUiThread() is specified then all callbacks
 * will be run on the UI thread.
 */
public class Permissions {
    private static final Queue<PermissionBlock> waiting = new LinkedList<>();
    private static final Queue<PermissionBlock> prompt = new LinkedList<>();

    private static PermissionsHelper permissionHelper = new PermissionsHelper();

    /**
     * Entry point for checking (and optionally prompting for) runtime permissions.
     *
     * Note: The provided context needs to be an Activity context in order to prompt. Use doNotPrompt()
     * for all other contexts.
     */
    public static PermissionBlock from(@NonNull Context context) {
        return new PermissionBlock(context, permissionHelper);
    }

    /**
     * This method will block until the specified permissions have been granted or denied by the user.
     * If needed the user will be prompted.
     *
     * @return true if all of the permissions have been granted. False if any of the permissions have been denied.
     */
    public static boolean waitFor(@NonNull Activity activity, String... permissions) {
        ThreadUtils.assertNotOnUiThread(); // We do not want to block the UI thread.

        // This task will block until all of the permissions have been granted
        final FutureTask<Boolean> blockingTask = new FutureTask<>(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return true;
            }
        });

        // This runnable will cancel the task if any of the permissions have been denied
        Runnable cancelBlockingTask = new Runnable() {
            @Override
            public void run() {
                blockingTask.cancel(true);
            }
        };

        Permissions.from(activity)
                .withPermissions(permissions)
                .andFallback(cancelBlockingTask)
                .run(blockingTask);

        try {
            return blockingTask.get();
        } catch (InterruptedException | ExecutionException | CancellationException e) {
            return false;
        }
    }

    /**
     * Determine whether you have been granted particular permissions.
     */
    public static boolean has(Context context, String... permissions) {
        return permissionHelper.hasPermissions(context, permissions);
    }

    /* package-private */ static void setPermissionHelper(PermissionsHelper permissionHelper) {
        Permissions.permissionHelper = permissionHelper;
    }

    /**
     * Callback for Activity.onRequestPermissionsResult(). All activities that prompt for permissions using this class
     * should implement onRequestPermissionsResult() and call this method.
     */
    public static synchronized void onRequestPermissionsResult(@NonNull Activity activity, @NonNull String[] permissions, @NonNull int[] grantResults) {
        processGrantResults(permissions, grantResults);

        processQueue(activity, permissions, grantResults);
    }

    /* package-private */ static synchronized void prompt(Activity activity, PermissionBlock block) {
        if (prompt.isEmpty()) {
            prompt.add(block);
            showPrompt(activity);
        } else {
            waiting.add(block);
        }
    }

    private static synchronized void processGrantResults(@NonNull String[] permissions, @NonNull int[] grantResults) {
        final HashSet<String> grantedPermissions = collectGrantedPermissions(permissions, grantResults);

        while (!prompt.isEmpty()) {
            final PermissionBlock block = prompt.poll();

            if (allPermissionsGranted(block, grantedPermissions)) {
                block.onPermissionsGranted();
            } else {
                block.onPermissionsDenied();
            }
        }
    }

    private static synchronized void processQueue(Activity activity, String[] permissions, int[] grantResults) {
        final HashSet<String> deniedPermissions = collectDeniedPermissions(permissions, grantResults);

        while (!waiting.isEmpty()) {
            final PermissionBlock block = waiting.poll();

            if (block.hasPermissions(activity)) {
                block.onPermissionsGranted();
            } else {
                if (atLeastOnePermissionDenied(block, deniedPermissions)) {
                    // We just prompted the user and one of the permissions of this block has been denied:
                    // There's no reason to instantly prompt again; Just reject without prompting.
                    block.onPermissionsDenied();
                } else {
                    prompt.add(block);
                }
            }
        }

        if (!prompt.isEmpty()) {
            showPrompt(activity);
        }
    }

    private static synchronized void showPrompt(Activity activity) {
        HashSet<String> permissions = new HashSet<>();

        for (PermissionBlock block : prompt) {
            Collections.addAll(permissions, block.getPermissions());
        }

        permissionHelper.prompt(activity, permissions.toArray(new String[permissions.size()]));
    }

    private static HashSet<String> collectGrantedPermissions(@NonNull String[] permissions, @NonNull int[] grantResults) {
        return filterPermissionsByResult(permissions, grantResults, PackageManager.PERMISSION_GRANTED);
    }

    private static HashSet<String> collectDeniedPermissions(@NonNull String[] permissions, @NonNull int[] grantResults) {
        return filterPermissionsByResult(permissions, grantResults, PackageManager.PERMISSION_DENIED);
    }

    private static HashSet<String> filterPermissionsByResult(@NonNull String[] permissions, @NonNull int[] grantResults, int result) {
        HashSet<String> grantedPermissions = new HashSet<>(permissions.length);
        for (int i = 0; i < permissions.length; i++) {
            if (grantResults[i] == result) {
                grantedPermissions.add(permissions[i]);
            }
        }
        return grantedPermissions;
    }

    private static boolean allPermissionsGranted(PermissionBlock block, HashSet<String> grantedPermissions) {
        for (String permission : block.getPermissions()) {
            if (!grantedPermissions.contains(permission)) {
                return false;
            }
        }

        return true;
    }

    private static boolean atLeastOnePermissionDenied(PermissionBlock block, HashSet<String> deniedPermissions) {
        for (String permission : block.getPermissions()) {
            if (deniedPermissions.contains(permission)) {
                return true;
            }
        }

        return false;
    }
}
