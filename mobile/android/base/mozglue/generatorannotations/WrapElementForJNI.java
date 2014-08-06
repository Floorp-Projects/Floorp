/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue.generatorannotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * This annotation is used to tag methods that are to have wrapper methods generated in the android
 * bridge. Such methods will be protected from destruction by Proguard, and allow us to avoid writing
 * by hand large amounts of boring boilerplate.
 * An annotated Java method will have a corresponding method generated in the android bridge.
 * By default, the name of the generated method will be the same as the name of the Java method, with
 * the first letter in upper case. The stubName property may be used to specify a custom name for the
 * generated method stub.
 *
 * allowMultithreaded should be used as sparingly as possible - the resulting code will allow the
 * Java method to be invoked from the C side from multiple threads. Often, this isn't what is wanted
 * and may lead to subtle bugs.
 */
@Target({ElementType.FIELD, ElementType.METHOD, ElementType.CONSTRUCTOR})
@Retention(RetentionPolicy.RUNTIME)
public @interface WrapElementForJNI {
    // Optional parameter specifying the name of the generated method stub. If omitted, the name
    // of the Java method will be used.
    String stubName() default "";

    /**
     * If set, the generated method stub will support being called from any thread via the use of
     * GetJNIForThread. This is rarely useful, at time of writing, as well as possibly risky.
     * See information in AndroidBridge.cpp regarding GetJNIForThread.
     *
     * Did I mention use of this function is discouraged?
     */
    boolean allowMultithread() default false;

    /**
     * If set, the generated stub will not handle uncaught exceptions.
     * Any exception must be handled or cleared by the code calling the stub.
     */
    boolean noThrow() default false;

    boolean narrowChars() default false;
}
