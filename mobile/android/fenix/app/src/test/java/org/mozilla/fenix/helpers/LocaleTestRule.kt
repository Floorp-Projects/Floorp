/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import org.junit.rules.TestRule
import org.junit.runner.Description
import org.junit.runners.model.Statement
import java.util.Locale

/**
 * A JUnit [TestRule] that sets the default locale to a given [localeToSet] for the duration of
 * the test and then resets it to the original locale.
 *
 * @param localeToSet The locale to set for the duration of the test.
 */
class LocaleTestRule(private val localeToSet: Locale) : TestRule {

    private var originalLocale: Locale? = null

    override fun apply(base: Statement, description: Description): Statement =
        object : Statement() {

            override fun evaluate() {
                originalLocale = Locale.getDefault()
                Locale.setDefault(localeToSet)

                try {
                    base.evaluate() // Run the tests
                } finally {
                    // Reset to the original locale after tests
                    Locale.setDefault(originalLocale!!)
                }
            }
        }
}
