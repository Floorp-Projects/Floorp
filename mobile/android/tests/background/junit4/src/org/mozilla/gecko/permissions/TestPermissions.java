/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.permissions;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Matchers;

import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.mockito.Mockito.*;

@RunWith(TestRunner.class)
public class TestPermissions {
    @Test
    public void testSuccessRunnableIsExecutedIfPermissionsAreGranted() {
        Permissions.setPermissionHelper(mockGrantingHelper());

        Runnable onPermissionsGranted = mock(Runnable.class);
        Runnable onPermissionsDenied = mock(Runnable.class);

        Permissions.from(mockActivity())
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .andFallback(onPermissionsDenied)
                .run(onPermissionsGranted);

        verify(onPermissionsDenied, never()).run();
        verify(onPermissionsGranted).run();
    }

    @Test
    public void testFallbackRunnableIsExecutedIfPermissionsAreDenied() {
        Permissions.setPermissionHelper(mockDenyingHelper());

        Runnable onPermissionsGranted = mock(Runnable.class);
        Runnable onPermissionsDenied = mock(Runnable.class);

        Activity activity = mockActivity();

        Permissions.from(activity)
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .andFallback(onPermissionsDenied)
                .run(onPermissionsGranted);

        Permissions.onRequestPermissionsResult(activity, new String[]{
                Manifest.permission.WRITE_EXTERNAL_STORAGE
        }, new int[]{
                PackageManager.PERMISSION_DENIED
        });

        verify(onPermissionsDenied).run();
        verify(onPermissionsGranted, never()).run();
    }

    @Test
    public void testPromptingForNotGrantedPermissions() {
        Activity activity = mockActivity();

        PermissionsHelper helper = mockDenyingHelper();
        Permissions.setPermissionHelper(helper);

        Permissions.from(activity)
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .andFallback(mock(Runnable.class))
                .run(mock(Runnable.class));

        verify(helper).prompt(anyActivity(), any(String[].class));

        Permissions.onRequestPermissionsResult(activity, new String[0], new int[0]);
    }

    @Test
    public void testMultipleRequestsAreQueuedAndDispatchedSequentially() {
        Activity activity = mockActivity();

        PermissionsHelper helper = mockDenyingHelper();
        Permissions.setPermissionHelper(helper);

        Runnable onFirstPermissionGranted = mock(Runnable.class);
        Runnable onSecondPermissionDenied = mock(Runnable.class);

        Permissions.from(activity)
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .andFallback(mock(Runnable.class))
                .run(onFirstPermissionGranted);

        Permissions.from(activity)
                .withPermissions(Manifest.permission.CAMERA)
                .andFallback(onSecondPermissionDenied)
                .run(mock(Runnable.class));


        Permissions.onRequestPermissionsResult(activity, new String[] {
                Manifest.permission.WRITE_EXTERNAL_STORAGE
        }, new int[] {
                PackageManager.PERMISSION_GRANTED
        });

        verify(onFirstPermissionGranted).run();
        verify(onSecondPermissionDenied, never()).run(); // Second request is queued but not executed yet

        Permissions.onRequestPermissionsResult(activity, new String[]{
                Manifest.permission.CAMERA
        }, new int[]{
                PackageManager.PERMISSION_DENIED
        });

        verify(onFirstPermissionGranted).run();
        verify(onSecondPermissionDenied).run();

        verify(helper, times(2)).prompt(anyActivity(), any(String[].class));
    }

    @Test
    public void testSecondRequestWillNotPromptIfPermissionHasBeenGranted() {
        Activity activity = mockActivity();

        PermissionsHelper helper = mock(PermissionsHelper.class);
        Permissions.setPermissionHelper(helper);
        when(helper.hasPermissions(anyContext(), anyPermissions()))
                .thenReturn(false)
                .thenReturn(false)
                .thenReturn(true); // Revaluation is successful

        Permissions.from(activity)
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .andFallback(mock(Runnable.class))
                .run(mock(Runnable.class));

        Permissions.from(activity)
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .andFallback(mock(Runnable.class))
                .run(mock(Runnable.class));

        Permissions.onRequestPermissionsResult(activity, new String[]{
                Manifest.permission.WRITE_EXTERNAL_STORAGE
        }, new int[]{
                PackageManager.PERMISSION_GRANTED
        });

        verify(helper, times(1)).prompt(anyActivity(), any(String[].class));
    }

    @Test
    public void testEmptyPermissionsArrayWillExecuteRunnableAndNotTryToPrompt() {
        PermissionsHelper helper = spy(new PermissionsHelper());
        Permissions.setPermissionHelper(helper);

        Runnable onPermissionGranted = mock(Runnable.class);
        Runnable onPermissionDenied = mock(Runnable.class);

        Permissions.from(mockActivity())
                .withPermissions()
                .andFallback(onPermissionDenied)
                .run(onPermissionGranted);

        verify(onPermissionGranted).run();
        verify(onPermissionDenied, never()).run();
        verify(helper, never()).prompt(anyActivity(), any(String[].class));
    }

    @Test
    public void testDoNotPromptBehavior() {
        PermissionsHelper helper = mockDenyingHelper();
        Permissions.setPermissionHelper(helper);

        Permissions.from(mockActivity())
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .doNotPrompt()
                .andFallback(mock(Runnable.class))
                .run(mock(Runnable.class));

        verify(helper, never()).prompt(anyActivity(), any(String[].class));
    }

    @Test(expected = IllegalStateException.class)
    public void testThrowsExceptionIfNeedstoPromptWithNonActivityContext() {
        Permissions.setPermissionHelper(mockDenyingHelper());

        Permissions.from(mock(Context.class))
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .andFallback(mock(Runnable.class))
                .run(mock(Runnable.class));
    }

    @Test
    public void testDoNotPromptIfFalse() {
        Activity activity = mockActivity();

        PermissionsHelper helper = mockDenyingHelper();
        Permissions.setPermissionHelper(helper);

        Permissions.from(activity)
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .doNotPromptIf(false)
                .andFallback(mock(Runnable.class))
                .run(mock(Runnable.class));

        verify(helper).prompt(anyActivity(), any(String[].class));

        Permissions.onRequestPermissionsResult(activity, new String[0], new int[0]);
    }

    @Test
    public void testDoNotPromptIfTrue() {
        PermissionsHelper helper = mockDenyingHelper();
        Permissions.setPermissionHelper(helper);

        Permissions.from(mockActivity())
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .doNotPromptIf(true)
                .andFallback(mock(Runnable.class))
                .run(mock(Runnable.class));

        verify(helper, never()).prompt(anyActivity(), any(String[].class));
    }

    private Activity mockActivity() {
        return mock(Activity.class);
    }

    private PermissionsHelper mockGrantingHelper() {
        PermissionsHelper helper = mock(PermissionsHelper.class);
        doReturn(true).when(helper).hasPermissions(any(Context.class), anyPermissions());
        return helper;
    }

    private PermissionsHelper mockDenyingHelper() {
        PermissionsHelper helper = mock(PermissionsHelper.class);
        doReturn(false).when(helper).hasPermissions(any(Context.class), anyPermissions());
        return helper;
    }

    private String anyPermissions() {
        return Matchers.anyVararg();
    }

    private Activity anyActivity() {
        return any(Activity.class);
    }

    private Context anyContext() {
        return any(Context.class);
    }
}
