/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotation;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * This annotation is used to tag classes that are conditionally built behind
 * build flags. Any generated JNI bindings will incorporate the specified build
 * flags.
 */
@Target({ElementType.TYPE})
@Retention(RetentionPolicy.RUNTIME)
public @interface BuildFlag {
    /**
     * Preprocessor macro for conditionally building the generated bindings.
     * "MOZ_FOO" wraps generated bindings in "#ifdef MOZ_FOO / #endif"
     * "!MOZ_FOO" wraps generated bindings in "#ifndef MOZ_FOO / #endif"
     */
    String value() default "";
}
