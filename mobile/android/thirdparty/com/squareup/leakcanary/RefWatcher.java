/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.squareup.leakcanary;

/**
 * No-op implementation of {@link RefWatcher} for release builds. Please use {@link
 * RefWatcher#DISABLED}.
 */
public final class RefWatcher {
    public static final RefWatcher DISABLED = new RefWatcher();

    private RefWatcher() {}

    public void watch(Object watchedReference) {}

    public void watch(Object watchedReference, String referenceName) {}
}
