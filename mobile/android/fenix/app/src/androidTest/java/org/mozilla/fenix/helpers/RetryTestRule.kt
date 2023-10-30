/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import androidx.test.espresso.IdlingResourceTimeoutException
import androidx.test.espresso.NoMatchingViewException
import androidx.test.uiautomator.UiObjectNotFoundException
import junit.framework.AssertionFailedError
import kotlinx.coroutines.runBlocking
import org.junit.rules.TestRule
import org.junit.runner.Description
import org.junit.runners.model.Statement
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.helpers.AppAndSystemHelper.setNetworkEnabled
import org.mozilla.fenix.helpers.IdlingResourceHelper.unregisterAllIdlingResources
import org.mozilla.fenix.helpers.TestHelper.appContext

/**
 *  Rule to retry flaky tests for a given number of times, catching some of the more common exceptions.
 *  The Rule doesn't clear the app state in between retries, so we are doing some cleanup here.
 *  The @Before and @After methods are not called between retries.
 *
 */
class RetryTestRule(private val retryCount: Int = 5) : TestRule {
    // Used for clearing all permission data after each test try
    private val permissionStorage = PermissionStorage(appContext.applicationContext)

    @Suppress("TooGenericExceptionCaught", "ComplexMethod")
    override fun apply(base: Statement, description: Description): Statement {
        return statement {
            for (i in 1..retryCount) {
                try {
                    base.evaluate()
                    break
                } catch (t: AssertionError) {
                    setNetworkEnabled(true)
                    unregisterAllIdlingResources()
                    runBlocking {
                        permissionStorage.deleteAllSitePermissions()
                    }
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: AssertionFailedError) {
                    unregisterAllIdlingResources()
                    runBlocking {
                        permissionStorage.deleteAllSitePermissions()
                    }
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: UiObjectNotFoundException) {
                    setNetworkEnabled(true)
                    unregisterAllIdlingResources()
                    runBlocking {
                        permissionStorage.deleteAllSitePermissions()
                    }
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: NoMatchingViewException) {
                    setNetworkEnabled(true)
                    unregisterAllIdlingResources()
                    runBlocking {
                        permissionStorage.deleteAllSitePermissions()
                    }
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: IdlingResourceTimeoutException) {
                    setNetworkEnabled(true)
                    unregisterAllIdlingResources()
                    runBlocking {
                        permissionStorage.deleteAllSitePermissions()
                    }
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: RuntimeException) {
                    setNetworkEnabled(true)
                    unregisterAllIdlingResources()
                    runBlocking {
                        permissionStorage.deleteAllSitePermissions()
                    }
                    if (i == retryCount) {
                        throw t
                    }
                } catch (t: NullPointerException) {
                    setNetworkEnabled(true)
                    unregisterAllIdlingResources()
                    runBlocking {
                        permissionStorage.deleteAllSitePermissions()
                    }
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
