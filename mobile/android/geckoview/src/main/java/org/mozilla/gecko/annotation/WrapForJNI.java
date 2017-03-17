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
     * - "abort" will cause a crash if there is a pending exception.
     * - "ignore" will not handle any pending exceptions; it is then the caller's
     *   responsibility to handle exceptions.
     * - "nsresult" will clear any pending exceptions and return an error code; not
     *   supported for native methods.
     */
    String exceptionMode() default "abort";

    /**
     * The thread that the method will be called from.
     * One of "any", "gecko", or "ui". Not supported for fields.
     */
    String calledFrom() default "any";

    /**
     * The thread that the method call will be dispatched to.
     * - "current" indicates no dispatching; only supported value for fields,
     *   constructors, non-native methods, and non-void native methods.
     * - "gecko" indicates dispatching to the Gecko XPCOM (nsThread) event queue.
     * - "gecko_priority" indicates dispatching to the Gecko widget
     *   (nsAppShell) event queue; in most cases, events in the widget event
     *   queue (aka native event queue) are favored over events in the XPCOM
     *   event queue.
     * - "proxy" indicates dispatching to a proxy function as a function object; see
     *   widget/jni/Natives.h.
     */
    String dispatchTo() default "current";
}
