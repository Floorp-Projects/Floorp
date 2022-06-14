/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import androidx.test.espresso.IdlingResourceTimeoutException
import androidx.test.espresso.NoMatchingViewException
import androidx.test.uiautomator.UiObjectNotFoundException
import junit.framework.AssertionFailedError
import org.junit.rules.TestRule
import org.junit.runner.Description
import org.junit.runners.model.Statement
import org.mozilla.fenix.helpers.IdlingResourceHelper.unregisterAllIdlingResources

class RetryTestRule(private val retryCount: Int = 5) : TestRule {

    @Suppress("TooGenericExceptionCaught", "ComplexMethod")
    override fun apply(base: Statement, description: Description): Statement {
        return statement {
            for (i in 1..retryCount) {
                try {
                    base.evaluate()
                    break
                } catch (t: AssertionError) {
                    unregisterAllIdlingResources()
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: AssertionFailedError) {
                    unregisterAllIdlingResources()
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: UiObjectNotFoundException) {
                    unregisterAllIdlingResources()
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: NoMatchingViewException) {
                    unregisterAllIdlingResources()
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: IdlingResourceTimeoutException) {
                    unregisterAllIdlingResources()
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: RuntimeException) {
                    unregisterAllIdlingResources()
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: NullPointerException) {
                    unregisterAllIdlingResources()
                    if (i == retryCount) {
                        throw t
                    }
                }
            }
        }
    }

    private inline fun statement(crossinline eval: () -> Unit): Statement {
        return object : Statement() {
            override fun evaluate() = eval()
        }
    }
}
