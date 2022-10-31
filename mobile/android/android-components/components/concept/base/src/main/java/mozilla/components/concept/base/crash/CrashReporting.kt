/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.base.crash

import kotlinx.coroutines.Job

/**
 * A  crash reporter interface that can report caught exception to multiple services.
 */
interface CrashReporting {

    /**
     * Submit a caught exception report to all registered services.
     */
    fun submitCaughtException(throwable: Throwable): Job

    /**
     * Add a crash breadcrumb to all registered services with breadcrumb support.
     */
    fun recordCrashBreadcrumb(breadcrumb: Breadcrumb)
}
