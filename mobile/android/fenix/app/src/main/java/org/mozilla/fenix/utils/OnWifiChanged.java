package org.mozilla.fenix.utils;

/**
 * Functional interface for listening to when wifi is/is not connected.
 *
 * This is not a () -> Boolean so that method parameters can be more clearly typed.
 *
 * This file is in Java because of the SAM conversion problem in Kotlin.
 * See https://youtrack.jetbrains.com/issue/KT-7770.
 */
@FunctionalInterface
public interface OnWifiChanged {
    void invoke(boolean Connected);
}
