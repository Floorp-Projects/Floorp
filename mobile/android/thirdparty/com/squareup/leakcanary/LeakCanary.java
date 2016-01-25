/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.squareup.leakcanary;

import android.app.Application;

/**
 * A no-op version of {@link LeakCanary} that can be used in release builds.
 */
public final class LeakCanary {
    public static RefWatcher install(Application application) {
        return RefWatcher.DISABLED;
    }

    private LeakCanary() {
        throw new AssertionError();
    }
}
