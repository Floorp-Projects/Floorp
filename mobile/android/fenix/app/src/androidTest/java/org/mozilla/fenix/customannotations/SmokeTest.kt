package org.mozilla.fenix.customannotations

/**
 * A custom annotation to mark the smoke tests corresponding to the ones in TestRail:
 * https://testrail.stage.mozaws.net/index.php?/suites/view/3192
 */
@Target(AnnotationTarget.FUNCTION, AnnotationTarget.CLASS)
@Retention(AnnotationRetention.RUNTIME)
annotation class SmokeTest
