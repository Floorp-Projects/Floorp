/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customannotations

/**
 * A custom annotation to mark the smoke tests corresponding to the ones in TestRail:
 * https://testrail.stage.mozaws.net/index.php?/suites/view/3192
 */
@Target(AnnotationTarget.FUNCTION, AnnotationTarget.CLASS)
@Retention(AnnotationRetention.RUNTIME)
annotation class SmokeTest
