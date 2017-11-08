/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.helpers;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;

import org.mozilla.gecko.AppConstants;
import org.robolectric.RuntimeEnvironment;

import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

public class MockUserManager {

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    public static Context getContextWithMockedUserManager() {
        // This seems to be done automatically in newer versions of RoboElectric, we need our own
        // version for now:
        final Context context;

        if (!AppConstants.Versions.preJBMR2) {
            context = spy(RuntimeEnvironment.application);

            final UserManager userManager = spy((UserManager) context.getSystemService(Context.USER_SERVICE));
            doReturn(new Bundle()).when(userManager).getApplicationRestrictions(anyString());
            doReturn(new Bundle()).when(userManager).getUserRestrictions();

            doReturn(userManager).when(context).getSystemService(Context.USER_SERVICE);
        } else {
            context = RuntimeEnvironment.application;
        }

        return context;
    }
}
