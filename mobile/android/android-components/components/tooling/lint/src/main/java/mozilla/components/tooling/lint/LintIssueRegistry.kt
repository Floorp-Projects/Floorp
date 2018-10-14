/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.client.api.IssueRegistry
import com.android.tools.lint.detector.api.Issue

/**
 * Registry which provides a list of our custom lint checks to be performed on an Android project.
 */
@Suppress("unused")
class LintIssueRegistry : IssueRegistry() {
    override val api: Int = com.android.tools.lint.detector.api.CURRENT_API
    override val issues: List<Issue> = LintLogChecks.issues
}
