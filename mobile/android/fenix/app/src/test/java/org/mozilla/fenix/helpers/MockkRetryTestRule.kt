/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import io.mockk.MockKException
import io.mockk.unmockkAll
import mozilla.components.support.base.log.logger.Logger
import org.junit.rules.TestRule
import org.junit.runner.Description
import org.junit.runners.model.Statement

/**
 * [TestRule] to work around mockk problem that causes intermittent failures
 * of tests with mocked lambdas. This rule will call `unmockAll` and retry
 * running the failing test until [maxTries] is reached.
 *
 * See:
 * https://github.com/mockk/mockk/issues/598
 * https://github.com/mozilla-mobile/fenix/issues/21952
 * https://github.com/mozilla-mobile/fenix/issues/22240
 */
class MockkRetryTestRule(val maxTries: Int = 3) : TestRule {

    private val logger = Logger("MockkRetryTestRule")

    @Suppress("TooGenericExceptionCaught", "NestedBlockDepth")
    override fun apply(base: Statement, description: Description): Statement {
        return object : Statement() {
            override fun evaluate() {
                var failure: Throwable? = null

                for (i in 0 until maxTries) {
                    try {
                        base.evaluate()
                        return
                    } catch (throwable: Throwable) {
                        when (throwable) {
                            // Work around intermittently failing tests with mocked lambdas
                            // on JDK 11: https://github.com/mockk/mockk/issues/598
                            is InstantiationError,
                            is MockKException,
                            -> {
                                failure = throwable
                                val message = if (i < maxTries - 1) {
                                    "Retrying test \"${description.displayName}\""
                                } else {
                                    "Giving up on test \"${description.displayName}\" after $maxTries tries"
                                }
                                logger.error(message, throwable)
                                unmockkAll()
                            }
                            else -> {
                                throw throwable
                            }
                        }
                    }
                }

                throw failure!!
            }
        }
    }
}
