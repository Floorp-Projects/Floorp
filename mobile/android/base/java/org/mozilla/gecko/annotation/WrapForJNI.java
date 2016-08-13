/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotation;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * This annotation is used to tag methods that are to have wrapper methods generated.
 * Such methods will be protected from destruction by ProGuard, and allow us to avoid
 * writing by hand large amounts of boring boilerplate.
 */
@Target({ElementType.TYPE, ElementType.FIELD, ElementType.METHOD, ElementType.CONSTRUCTOR})
@Retention(RetentionPolicy.RUNTIME)
public @interface WrapForJNI {
    /**
     * Skip this member when generating wrappers for a whole class.
     */
    boolean skip() default false;

    /**
     * Optional parameter specifying the name of the generated method stub. If omitted,
     * the capitalized name of the Java method will be used.
     */
    String stubName() default "";

    /**
     * Action to take if member access returns an exception.
     * One of "abort", "ignore", or "nsresult". "nsresult" is not supported for native
     * methods.
     */
    String exceptionMode() default "abort";

    /**
     * The thread that the method will be called from.
     * One of "any", "gecko", or "ui". Not supported for fields.
     */
    String calledFrom() default "any";

    /**
     * The thread that the method call will be dispatched to.
     * One of "current", "gecko", or "proxy". Not supported for non-native methods,
     * fields, and constructors. Only void-return methods are supported for anything other
     * than current thread.
     */
    String dispatchTo() default "current";
}
