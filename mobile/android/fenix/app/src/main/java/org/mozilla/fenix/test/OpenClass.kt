/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.test

/**
 * Annotate a class with [OpenClass] to open a class for mocking purposes while keeping it final in release builds
 */
@Target(AnnotationTarget.ANNOTATION_CLASS)
annotation class OpenClass

/**
 * Annotate a class with [Mockable] to make it extensible in debug builds
 */
@OpenClass
@Target(AnnotationTarget.CLASS)
annotation class Mockable
