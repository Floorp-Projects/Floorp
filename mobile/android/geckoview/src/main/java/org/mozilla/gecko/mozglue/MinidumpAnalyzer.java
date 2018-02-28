/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue;

/**
 * JNI wrapper for accessing the minidump analyzer tool. This is used to
 * generate stack traces and other process information from a crash minidump.
 */
public final class MinidumpAnalyzer {
    private MinidumpAnalyzer() {
        // prevent instantiation
    }

    /**
     * Generate the stacks from the minidump file specified in minidumpPath
     * and adds the StackTraces annotation to the associated .extra file.
     * If fullStacks is false then only the stack trace for the crashing thread
     * will be generated, otherwise stacks will be generated for all threads.
     *
     * This JNI method is implemented in mozglue/android/nsGeckoUtils.cpp.
     *
     * @param minidumpPath The path to the minidump file to be analyzed.
     * @param fullStacks Specifies if stacks must be generated for all threads.
     * @return <code>true</code> if the operation was successful,
     *         <code>false</code> otherwise.
     */
    public static native boolean GenerateStacks(String minidumpPath, boolean fullStacks);
}
