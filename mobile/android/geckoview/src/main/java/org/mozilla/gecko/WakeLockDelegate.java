/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

/**
 * A <code>WakeLockDelegate</code> is responsible for acquiring and release wake-locks.
 */
public interface WakeLockDelegate {
    /**
     * Wake-lock for the CPU.
     */
    final String LOCK_CPU = "cpu";
    /**
     * Wake-lock for the screen.
     */
    final String LOCK_SCREEN = "screen";

    final int LOCKS_COUNT = 2;

    /**
     * No one holds the wake-lock.
     */
    final int STATE_UNLOCKED = 0;
    /**
     * The wake-lock is held by a foreground window.
     */
    final int STATE_LOCKED_FOREGROUND = 1;
    /**
     * The wake-lock is held by a background window.
     */
    final int STATE_LOCKED_BACKGROUND = 2;

    /**
     * Set a wake-lock to a specified state. Called from the Gecko thread.
     *
     * @param lock Wake-lock to set from one of the LOCK_* constants.
     * @param state New wake-lock state from one of the STATE_* constants.
     */
    void setWakeLockState(String lock, int state);
}
