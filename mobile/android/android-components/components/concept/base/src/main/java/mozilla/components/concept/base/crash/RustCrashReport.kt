/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.base.crash

/**
 * Crash report for rust errors
 *
 * We implement this on exception classes that correspond to Rust errors to
 * customize how the crash reports look.
 *
 * CrashReporting implementors should test if exceptions implement this
 * interface.  If so, they should try to customize their crash reports to match.
 */
interface RustCrashReport {
    val typeName: String
    val message: String
}
