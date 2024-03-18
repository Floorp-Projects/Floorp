/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test.helpers

import org.hamcrest.Matcher
import org.junit.Assert.assertEquals

/**
 * An [assertEquals] method for hamcrest [Matcher]s. This is necessary because [Matcher]s do not define a
 * .equals method: instead, we compare the String description of two matchers to discover if they're equivalent.
 * This has some gotchas, e.g. functionally `not not Matcher == Matcher` but in a String description, they do not.
 */
// We don't name shadow assertEquals because then importing both requires the consumer to rename the imports.
fun <T> assertEqualsMatchers(expected: Matcher<T>, actual: Matcher<T>) {
    assertEquals(expected.toString(), actual.toString())
}
