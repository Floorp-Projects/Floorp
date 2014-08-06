/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue.generatorannotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Used to annotate parameters which are optional on the C++ side of the bridge. The annotation is
 * used by the annotation processor to generate the appropriate C++ headers so calls into the Java
 * method all have the optional params set to the default value.
 * The default values are zero for numerical types, false for booleans, "" for strings, and null
 * for all other reference types.
 */
@Target({ElementType.PARAMETER})
@Retention(RetentionPolicy.RUNTIME)
public @interface OptionalGeneratedParameter {}
